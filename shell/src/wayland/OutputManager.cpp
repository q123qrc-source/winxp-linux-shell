// SPDX-License-Identifier: GPL-3.0-or-later
#include "OutputManager.h"

#include "qwayland-wlr-output-management-unstable-v1.h"

#include <QGuiApplication>
#include <QHash>
#include <QLoggingCategory>
#include <QPointer>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>

#include <cmath>

Q_LOGGING_CATEGORY(lcOutput, "meadow.shell.output")

namespace {

class ModeObject : public QtWayland::zwlr_output_mode_v1
{
public:
    explicit ModeObject(::zwlr_output_mode_v1 *object) : QtWayland::zwlr_output_mode_v1(object) {}
    ~ModeObject() override
    {
        if (zwlr_output_mode_v1::version() >= 3)
            release();
        else
            zwlr_output_mode_v1_destroy(object());
    }
    OutputManager::Mode mode;

protected:
    void zwlr_output_mode_v1_size(int32_t w, int32_t h) override { mode.width = w; mode.height = h; }
    void zwlr_output_mode_v1_refresh(int32_t r) override { mode.refresh = r; }
    void zwlr_output_mode_v1_preferred() override { mode.preferred = true; }
};

class HeadObject : public QtWayland::zwlr_output_head_v1
{
public:
    explicit HeadObject(::zwlr_output_head_v1 *object) : QtWayland::zwlr_output_head_v1(object) {}
    ~HeadObject() override
    {
        qDeleteAll(modes);
        if (zwlr_output_head_v1::version() >= 3)
            release();
        else
            zwlr_output_head_v1_destroy(object());
    }

    OutputManager::Output out;
    QList<ModeObject *> modes;
    ModeObject *currentMode = nullptr;
    bool gone = false;

    ModeObject *findMode(const OutputManager::Mode &m) const
    {
        ModeObject *best = nullptr;
        for (ModeObject *mo : modes) {
            if (mo->mode.width != m.width || mo->mode.height != m.height)
                continue;
            if (m.refresh == 0 ? (!best || mo->mode.refresh > best->mode.refresh) : mo->mode.refresh == m.refresh)
                best = mo;
        }
        return best;
    }

protected:
    void zwlr_output_head_v1_name(const QString &name) override { out.name = name; }
    void zwlr_output_head_v1_description(const QString &d) override { out.description = d; }
    void zwlr_output_head_v1_mode(struct ::zwlr_output_mode_v1 *mode) override { modes << new ModeObject(mode); }
    void zwlr_output_head_v1_enabled(int32_t enabled) override
    {
        out.enabled = enabled;
        if (!enabled)
            currentMode = nullptr;
    }
    void zwlr_output_head_v1_current_mode(struct ::zwlr_output_mode_v1 *mode) override
    {
        currentMode = nullptr;
        for (ModeObject *m : std::as_const(modes)) {
            if (m->object() == mode)
                currentMode = m;
        }
    }
    void zwlr_output_head_v1_position(int32_t x, int32_t y) override { out.x = x; out.y = y; }
    void zwlr_output_head_v1_scale(wl_fixed_t scale) override { out.scale = wl_fixed_to_double(scale); }
    void zwlr_output_head_v1_finished() override { gone = true; }
};

} // namespace

class OutputManagerObject : public QWaylandClientExtensionTemplate<OutputManagerObject>,
                            public QtWayland::zwlr_output_manager_v1
{
public:
    explicit OutputManagerObject(OutputManager *owner)
        : QWaylandClientExtensionTemplate<OutputManagerObject>(4)
        , m_owner(owner)
    {
        initialize();
    }
    ~OutputManagerObject() override
    {
        qDeleteAll(heads);
        if (isActive())
            stop();
    }

    QList<HeadObject *> heads;
    uint32_t serial = 0;

protected:
    void zwlr_output_manager_v1_head(struct ::zwlr_output_head_v1 *head) override { heads << new HeadObject(head); }
    void zwlr_output_manager_v1_done(uint32_t s) override
    {
        serial = s;
        for (int i = int(heads.size()) - 1; i >= 0; --i) {
            if (heads.at(i)->gone)
                delete heads.takeAt(i);
        }
        QList<OutputManager::Output> outs;
        for (HeadObject *h : std::as_const(heads)) {
            OutputManager::Output o = h->out;
            o.modes.clear();
            for (ModeObject *m : std::as_const(h->modes))
                o.modes << m->mode;
            o.current = h->currentMode ? h->currentMode->mode : OutputManager::Mode{};
            outs << o;
        }
        m_owner->setOutputs(outs);
    }
    void zwlr_output_manager_v1_finished() override {}

private:
    OutputManager *m_owner;
};

// One zwlr_output_configuration_v1: test, then apply on success.
class ConfigObject : public QObject, public QtWayland::zwlr_output_configuration_v1
{
    Q_OBJECT
public:
    explicit ConfigObject(::zwlr_output_configuration_v1 *object) : QtWayland::zwlr_output_configuration_v1(object) {}
    ~ConfigObject() override
    {
        destroy();
        // Configuration heads have no destructor request; free the proxies.
        for (::zwlr_output_configuration_head_v1 *h : std::as_const(heads))
            wl_proxy_destroy(reinterpret_cast<wl_proxy *>(h));
    }
    QList<::zwlr_output_configuration_head_v1 *> heads;

Q_SIGNALS:
    void result(int outcome); // 0 succeeded, 1 failed, 2 cancelled

protected:
    void zwlr_output_configuration_v1_succeeded() override { Q_EMIT result(0); }
    void zwlr_output_configuration_v1_failed() override { Q_EMIT result(1); }
    void zwlr_output_configuration_v1_cancelled() override { Q_EMIT result(2); }
};

struct OutputManager::Wayland
{
    std::unique_ptr<OutputManagerObject> manager;
};

OutputManager::OutputManager(bool simulate, QObject *parent)
    : QObject(parent)
    , m_simulate(simulate || QGuiApplication::platformName() != QLatin1String("wayland"))
{
    if (m_simulate) {
        Output o;
        o.name = QStringLiteral("Virtual-1");
        o.description = tr("Plug and Play Monitor");
        for (auto [w, h] : {std::pair{1280, 1024}, {1024, 768}, {800, 600}})
            o.modes << Mode{w, h, 60000, w == 1024};
        o.current = o.modes.at(1);
        m_outputs << o;
        return;
    }
    m_wl = std::make_unique<Wayland>();
    m_wl->manager = std::make_unique<OutputManagerObject>(this);
    connect(m_wl->manager.get(), &QWaylandClientExtension::activeChanged, this, &OutputManager::availableChanged);
}

OutputManager::~OutputManager() = default;

bool OutputManager::isAvailable() const
{
    return m_simulate || (m_wl && m_wl->manager->isActive());
}

void OutputManager::setOutputs(const QList<Output> &outputs)
{
    m_outputs = outputs;
    Q_EMIT outputsChanged();
}

QVariantList OutputManager::outputs() const
{
    QVariantList list;
    for (const Output &o : m_outputs) {
        QVariantList modes;
        int current = -1;
        for (int i = 0; i < o.modes.size(); ++i) {
            const Mode &m = o.modes.at(i);
            modes << QVariantMap{{QStringLiteral("width"), m.width}, {QStringLiteral("height"), m.height},
                                 {QStringLiteral("refresh"), m.refresh}, {QStringLiteral("preferred"), m.preferred}};
            if (m == o.current)
                current = i;
        }
        list << QVariantMap{{QStringLiteral("name"), o.name},
                            {QStringLiteral("description"), o.description},
                            {QStringLiteral("enabled"), o.enabled},
                            {QStringLiteral("x"), o.x},
                            {QStringLiteral("y"), o.y},
                            {QStringLiteral("width"), o.current.width},
                            {QStringLiteral("height"), o.current.height},
                            {QStringLiteral("refresh"), o.current.refresh},
                            {QStringLiteral("scale"), o.scale},
                            {QStringLiteral("currentMode"), current},
                            {QStringLiteral("modes"), modes}};
    }
    return list;
}

void OutputManager::apply(const QString &name, int width, int height, int refresh, double scale)
{
    QList<Output> target = m_outputs;
    bool found = false;
    for (Output &o : target) {
        if (o.name != name)
            continue;
        found = true;
        o.enabled = true;
        o.current = Mode{width, height, refresh, false};
        o.scale = scale > 0 ? scale : 1.0;
    }
    if (!found) {
        Q_EMIT applied(false, tr("The display was disconnected."));
        return;
    }
    applyConfiguration(target, true);
}

void OutputManager::revert()
{
    if (m_previous.isEmpty())
        return;
    const QList<Output> previous = m_previous;
    m_previous.clear();
    applyConfiguration(previous, false);
}

void OutputManager::keep()
{
    m_previous.clear();
    Q_EMIT outputsChanged();
}

void OutputManager::applyConfiguration(const QList<Output> &target, bool remember)
{
    const QList<Output> before = m_outputs;
    if (m_simulate) {
        qCInfo(lcOutput) << "apply (simulated)";
        if (remember)
            m_previous = before;
        for (const Output &t : target) {
            for (Output &o : m_outputs) {
                if (o.name == t.name) {
                    o.current = t.current;
                    for (const Mode &m : std::as_const(o.modes)) {
                        if (m.width == t.current.width && m.height == t.current.height)
                            o.current = m;
                    }
                    o.scale = t.scale;
                }
            }
        }
        Q_EMIT outputsChanged();
        Q_EMIT applied(true, {});
        return;
    }
    OutputManagerObject *mgr = m_wl ? m_wl->manager.get() : nullptr;
    if (!mgr || !mgr->isActive()) {
        Q_EMIT applied(false, tr("The compositor does not support changing display settings."));
        return;
    }

    // Builds a configuration for `target`; test=true only tests it.
    auto build = [mgr, target](bool test) -> ConfigObject * {
        auto *config = new ConfigObject(mgr->create_configuration(mgr->serial));
        for (HeadObject *head : std::as_const(mgr->heads)) {
            const Output *t = nullptr;
            for (const Output &o : target) {
                if (o.name == head->out.name)
                    t = &o;
            }
            if (!t || !t->enabled) {
                config->disable_head(head->object());
                continue;
            }
            ::zwlr_output_configuration_head_v1 *raw = config->enable_head(head->object());
            config->heads << raw;
            QtWayland::zwlr_output_configuration_head_v1 ch(raw);
            if (ModeObject *mode = head->findMode(t->current))
                ch.set_mode(mode->object());
            else
                ch.set_custom_mode(t->current.width, t->current.height, t->current.refresh);
            ch.set_position(t->x, t->y);
            ch.set_scale(wl_fixed_from_double(t->scale));
        }
        if (test)
            config->test();
        else
            config->apply();
        return config;
    };

    qCInfo(lcOutput) << "testing new display configuration";
    ConfigObject *testConfig = build(true);
    QPointer<OutputManager> self(this);
    connect(testConfig, &ConfigObject::result, this, [this, self, testConfig, build, before, remember](int outcome) {
        testConfig->deleteLater();
        if (!self)
            return;
        if (outcome != 0) {
            qCWarning(lcOutput) << "display configuration rejected" << outcome;
            Q_EMIT applied(false, outcome == 2 ? tr("The displays changed; try again.")
                                               : tr("This display does not support these settings."));
            return;
        }
        ConfigObject *applyConfig = build(false);
        connect(applyConfig, &ConfigObject::result, this, [this, applyConfig, before, remember](int result) {
            applyConfig->deleteLater();
            if (result == 0 && remember)
                m_previous = before;
            qCInfo(lcOutput) << "display configuration" << (result == 0 ? "applied" : "failed");
            Q_EMIT outputsChanged();
            Q_EMIT applied(result == 0, result == 0 ? QString() : tr("The display settings could not be applied."));
        });
    });
}

#include "OutputManager.moc"
