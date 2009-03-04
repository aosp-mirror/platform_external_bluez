#include <private/android_filesystem_config.h>
#include <sys/prctl.h>
#include <linux/capability.h>

/* Set UID to bluetooth w/ CAP_NET_RAW, CAP_NET_ADMIN and CAP_NET_BIND_SERVICE
 * (Android's init.rc does not yet support applying linux capabilities) */
void android_set_aid_and_cap() {
    prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);
    setuid(AID_BLUETOOTH);

    struct __user_cap_header_struct header;
    struct __user_cap_data_struct cap;
    header.version = _LINUX_CAPABILITY_VERSION;
    header.pid = 0;
    cap.effective = cap.permitted = 1 << CAP_NET_RAW |
                                    1 << CAP_NET_ADMIN |
                                    1 << CAP_NET_BIND_SERVICE;
    cap.inheritable = 0;
    capset(&header, &cap);
}
