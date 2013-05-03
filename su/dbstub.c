#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sqlite3.h>
#include <time.h>

#include "../../../external/koush/Superuser/Superuser/jni/su/su.h"

policy_t database_check(struct su_context *ctx) {
    return ALLOW;
}
