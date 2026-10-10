// SPDX-License-Identifier: GPL-3.0-or-later
// Stand-ins for the objects SDDM 0.21 exposes to greeter themes (sddm,
// userModel, sessionModel, keyboard, screenModel, config). Property, role and
// signal names match SDDM's GreeterProxy, UserModel, SessionModel,
// KeyboardModel, ScreenModel and ThemeConfig.
#pragma once

#include <QAbstractListModel>
#include <QQmlPropertyMap>
#include <QRect>
#include <QStringList>

class MockGreeterProxy : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString hostName READ hostName CONSTANT)
    Q_PROPERTY(bool canPowerOff READ canPower CONSTANT)
    Q_PROPERTY(bool canReboot READ canPower CONSTANT)
    Q_PROPERTY(bool canSuspend READ canPower CONSTANT)
    Q_PROPERTY(bool canHibernate READ canPower CONSTANT)
    Q_PROPERTY(bool canHybridSleep READ canPower CONSTANT)

public:
    explicit MockGreeterProxy(bool powerActions, QObject *parent = nullptr);
    QString hostName() const { return QStringLiteral("meadow-preview"); }
    bool canPower() const { return m_power; }

    // The one password the mock accepts.
    static constexpr const char *kPassword = "meadow";

public Q_SLOTS:
    void powerOff();
    void reboot();
    void suspend();
    void hibernate();
    void hybridSleep();
    void login(const QString &user, const QString &password, int sessionIndex);

Q_SIGNALS:
    void informationMessage(const QString &message);
    void loginFailed();
    void loginSucceeded();

private:
    bool m_power;
};

struct MockUser {
    QString name;
    QString realName;
    QString icon;
    bool needsPassword;
};

class MockUserModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int lastIndex READ lastIndex CONSTANT)
    Q_PROPERTY(QString lastUser READ lastUser CONSTANT)
    Q_PROPERTY(int count READ count CONSTANT)
    Q_PROPERTY(int disableAvatarsThreshold READ disableAvatarsThreshold CONSTANT)
    Q_PROPERTY(bool containsAllUsers READ containsAllUsers CONSTANT)

public:
    enum Roles { NameRole = Qt::UserRole + 1, RealNameRole, HomeDirRole, IconRole, NeedsPasswordRole };
    MockUserModel(const QList<MockUser> &users, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int lastIndex() const { return 0; }
    QString lastUser() const { return m_users.isEmpty() ? QString() : m_users.first().name; }
    int count() const { return int(m_users.size()); }
    int disableAvatarsThreshold() const { return 7; }
    bool containsAllUsers() const { return true; }

private:
    QList<MockUser> m_users;
};

class MockSessionModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int lastIndex READ lastIndex CONSTANT)
    Q_PROPERTY(int count READ count CONSTANT)

public:
    enum Roles { DirectoryRole = Qt::UserRole + 1, FileRole, TypeRole, NameRole, ExecRole, CommentRole };
    explicit MockSessionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int lastIndex() const { return 0; }
    int count() const { return int(m_names.size()); }

private:
    QStringList m_names;
};

class MockKeyboardLayout : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString shortName MEMBER m_short CONSTANT)
    Q_PROPERTY(QString longName MEMBER m_long CONSTANT)

public:
    MockKeyboardLayout(const QString &s, const QString &l, QObject *parent)
        : QObject(parent), m_short(s), m_long(l) {}

private:
    QString m_short;
    QString m_long;
};

class MockKeyboardModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool numLock MEMBER m_numLock NOTIFY numLockStateChanged)
    Q_PROPERTY(bool capsLock MEMBER m_capsLock NOTIFY capsLockStateChanged)
    Q_PROPERTY(int currentLayout READ currentLayout WRITE setCurrentLayout NOTIFY currentLayoutChanged)
    Q_PROPERTY(QList<QObject *> layouts READ layouts NOTIFY layoutsChanged)
    Q_PROPERTY(bool enabled READ enabled CONSTANT)

public:
    explicit MockKeyboardModel(QObject *parent = nullptr);
    int currentLayout() const { return m_current; }
    void setCurrentLayout(int id);
    QList<QObject *> layouts() const { return m_layouts; }
    bool enabled() const { return true; }

Q_SIGNALS:
    void numLockStateChanged();
    void capsLockStateChanged();
    void currentLayoutChanged();
    void layoutsChanged();

private:
    bool m_numLock = true;
    bool m_capsLock = false;
    int m_current = 0;
    QList<QObject *> m_layouts;
};

class MockScreenModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int primary READ primary CONSTANT)

public:
    MockScreenModel(const QRect &geometry, QObject *parent = nullptr)
        : QAbstractListModel(parent), m_geometry(geometry) {}
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : 1; }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int primary() const { return 0; }

private:
    QRect m_geometry;
};

// theme.conf access as in SDDM's ThemeConfig: properties plus typed getters.
class MockThemeConfig : public QQmlPropertyMap
{
    Q_OBJECT

public:
    MockThemeConfig(const QString &themeConf, const QStringList &overrides, QObject *parent = nullptr);
    Q_INVOKABLE bool boolValue(const QString &key) { return value(key).toBool(); }
    Q_INVOKABLE int intValue(const QString &key) { return value(key).toInt(); }
    Q_INVOKABLE qreal realValue(const QString &key) { return value(key).toReal(); }
    Q_INVOKABLE QString stringValue(const QString &key) { return value(key).toString(); }
};
