/* test.c: Description
 *
 * Copyright: (c) 2016 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2016-07-25
 * Version:   $Id: test.c 385 2017-01-16 10:25:48Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <libmx.h>

double t0;

int ctrl_timer = 0;
int test_timer = 1;

void on_test_timer(MX *mx, uint32_t id, double t, void *udata)
{
    printf("Timer %d timed out at t0 + %.1f sec.\n", id, t - t0);
}

void step5(MX *mx, uint32_t id, double t, void *udata)
{
    printf("%s at t0 + %.1f sec.\n", __func__, t - t0);

    mxShutdown(mx);
}

void step4(MX *mx, uint32_t id, double t, void *udata)
{
    printf("%s at t0 + %.1f sec.\n", __func__, t - t0);

    /* The test timer is again 1 second in our future. Let's try to remove it
     * and make sure it doesn't time out. Then, in 2 seconds we'll shut things
     * down. */

    mxRemoveTimer(mx, test_timer);
    mxCreateTimer(mx, ctrl_timer, t + 2.0, step5, NULL);
}

void step3(MX *mx, uint32_t id, double t, void *udata)
{
    printf("%s at t0 + %.1f sec.\n", __func__, t - t0);

    /* Let's create another test timer for 2 seconds in the future, and let's
     * try to remove it in 1 seconds. */

    mxCreateTimer(mx, test_timer, t + 2.0, on_test_timer, NULL);
    mxCreateTimer(mx, ctrl_timer, t + 1.0, step4, NULL);
}

void step2(MX *mx, uint32_t id, double t, void *udata)
{
    printf("%s at t0 + %.1f sec.\n", __func__, t - t0);

    /* The test timer is still 1 second in our future. Adjust it to 2 seconds in
     * the future and allow it to time out. Then move on to step3. */

    mxAdjustTimer(mx, test_timer, t + 2.0);
    mxCreateTimer(mx, ctrl_timer, t + 3.0, step3, NULL);
}

void step1(MX *mx, uint32_t id, double t, void *udata)
{
    printf("%s at t0 + %.1f sec.\n", __func__, t - t0);

    /* Let's start a test timer for 2 seconds from now. In 1 second we'll try
     * and adjust it. */

    mxCreateTimer(mx, test_timer, t + 2.0, on_test_timer, NULL);
    mxCreateTimer(mx, ctrl_timer, t + 1.0, step2, NULL);
}

int main(int argc, char *argv[])
{
    MX *mx = mxClient("localhost", NULL, "Test");

    if (mx == NULL) {
        printf("Error: %s", mxError());
        return -1;
    }

    t0 = mxNow();

    mxCreateTimer(mx, ctrl_timer, mxNow() + 1.0, step1, NULL);

    int r = mxRun(mx);

    mxDestroy(mx);

    return r;
}
