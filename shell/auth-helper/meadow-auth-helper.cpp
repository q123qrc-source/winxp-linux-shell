// SPDX-License-Identifier: GPL-3.0-or-later
// meadow-auth-helper: runs one PAM authentication for the user who started
// it, on behalf of meadow-shell's lock screen and polkit agent. Keeping PAM in
// its own short-lived process means a crashing or hanging PAM module cannot
// take the shell (and with it the session lock) down.
//
// Not setuid and needs no privileges: pam_unix checks the caller's own
// password through unix_chkpwd, as every screen locker does.
//
// The user is always the caller (getuid()); there is no way to ask for
// another account. The PAM service is "meadow-lock", or $MEADOW_PAM_SERVICE
// (used by the tests with pam_wrapper; it cannot raise any privilege since
// the helper runs as the caller).
//
// Protocol (stdin/stdout), one frame per message:
//     <TYPE> <length>\n<length bytes of UTF-8 payload>
// helper -> shell:  PROMPT_ECHO_OFF, PROMPT_ECHO_ON, INFO, ERROR (payload: the
//                   PAM message), RESULT (payload "ok" or "fail")
// shell -> helper:  RESPONSE (payload: the answer to the last prompt),
//                   CANCEL (empty; ends the conversation)
// The exit status is 0 on success, 1 on failure, 2 on a protocol error.
//
// Nothing the user types and no PAM message is ever written to stderr or a
// log: PAM messages can contain secrets (e.g. hints) and are only forwarded
// to the shell.
#include <security/pam_appl.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pwd.h>
#include <string>
#include <sys/prctl.h>
#include <unistd.h>

namespace {

constexpr size_t kMaxPayload = 4096;

bool writeAll(int fd, const char *data, size_t size)
{
    while (size > 0) {
        const ssize_t n = ::write(fd, data, size);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        data += n;
        size -= static_cast<size_t>(n);
    }
    return true;
}

bool sendFrame(const char *type, const char *payload)
{
    const size_t len = payload ? std::strlen(payload) : 0;
    char header[64];
    const int h = std::snprintf(header, sizeof header, "%s %zu\n", type, len);
    return h > 0 && writeAll(STDOUT_FILENO, header, static_cast<size_t>(h))
        && (len == 0 || writeAll(STDOUT_FILENO, payload, len));
}

bool readByte(char *c)
{
    for (;;) {
        const ssize_t n = ::read(STDIN_FILENO, c, 1);
        if (n == 1)
            return true;
        if (n < 0 && errno == EINTR)
            continue;
        return false;
    }
}

// Reads one frame. The payload is returned in a malloc'd buffer (handed to
// PAM as a response, which frees it) or nullptr for an empty payload.
bool readFrame(std::string *type, char **payload)
{
    std::string header;
    char c;
    while (readByte(&c) && c != '\n') {
        if (header.size() > 64)
            return false;
        header.push_back(c);
    }
    if (c != '\n')
        return false;
    const size_t space = header.find(' ');
    if (space == std::string::npos)
        return false;
    *type = header.substr(0, space);
    char *end = nullptr;
    const unsigned long len = std::strtoul(header.c_str() + space + 1, &end, 10);
    if (!end || *end != '\0' || len > kMaxPayload)
        return false;
    char *buf = static_cast<char *>(std::calloc(len + 1, 1));
    if (!buf)
        return false;
    for (unsigned long i = 0; i < len; ++i) {
        if (!readByte(&buf[i])) {
            explicit_bzero(buf, len);
            std::free(buf);
            return false;
        }
    }
    *payload = buf;
    return true;
}

int conversation(int count, const struct pam_message **messages, struct pam_response **responses, void *)
{
    if (count <= 0 || count > PAM_MAX_NUM_MSG)
        return PAM_CONV_ERR;
    auto *replies = static_cast<pam_response *>(std::calloc(static_cast<size_t>(count), sizeof(pam_response)));
    if (!replies)
        return PAM_BUF_ERR;

    auto fail = [&] {
        for (int i = 0; i < count; ++i) {
            if (replies[i].resp) {
                explicit_bzero(replies[i].resp, std::strlen(replies[i].resp));
                std::free(replies[i].resp);
            }
        }
        std::free(replies);
        return PAM_CONV_ERR;
    };

    for (int i = 0; i < count; ++i) {
        const pam_message *m = messages[i];
        switch (m->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
        case PAM_PROMPT_ECHO_ON: {
            if (!sendFrame(m->msg_style == PAM_PROMPT_ECHO_OFF ? "PROMPT_ECHO_OFF" : "PROMPT_ECHO_ON", m->msg))
                return fail();
            std::string type;
            char *answer = nullptr;
            if (!readFrame(&type, &answer))
                return fail();
            if (type != "RESPONSE") {
                if (answer) {
                    explicit_bzero(answer, std::strlen(answer));
                    std::free(answer);
                }
                return fail();
            }
            replies[i].resp = answer;
            break;
        }
        case PAM_TEXT_INFO:
            if (!sendFrame("INFO", m->msg))
                return fail();
            break;
        case PAM_ERROR_MSG:
            if (!sendFrame("ERROR", m->msg))
                return fail();
            break;
        default:
            return fail();
        }
    }
    *responses = replies;
    return PAM_SUCCESS;
}

} // namespace

int main(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        std::fputs("usage: meadow-auth-helper (driven by meadow-shell over stdin/stdout)\n", stderr);
        return 2;
    }
    // No core dumps or ptrace by other processes of the user while a password
    // may be in memory.
    prctl(PR_SET_DUMPABLE, 0);

    const passwd *pw = getpwuid(getuid());
    if (!pw || !pw->pw_name) {
        std::fputs("meadow-auth-helper: cannot determine the calling user\n", stderr);
        sendFrame("RESULT", "fail");
        return 1;
    }
    const std::string user = pw->pw_name;
    const char *serviceEnv = std::getenv("MEADOW_PAM_SERVICE");
    const std::string service = serviceEnv && *serviceEnv ? serviceEnv : "meadow-lock";

    const pam_conv conv = {conversation, nullptr};
    pam_handle_t *pamh = nullptr;
    int rc = pam_start(service.c_str(), user.c_str(), &conv, &pamh);
    if (rc == PAM_SUCCESS)
        rc = pam_authenticate(pamh, 0);
    if (rc == PAM_SUCCESS)
        rc = pam_acct_mgmt(pamh, 0);
    if (rc == PAM_SUCCESS)
        pam_setcred(pamh, PAM_REFRESH_CRED); // best effort (e.g. Kerberos tickets)
    const bool ok = rc == PAM_SUCCESS;
    if (pamh)
        pam_end(pamh, rc);

    // Only the outcome is reported; never the PAM error text.
    std::fprintf(stderr, "meadow-auth-helper: authentication %s\n", ok ? "succeeded" : "failed");
    sendFrame("RESULT", ok ? "ok" : "fail");
    return ok ? 0 : 1;
}
