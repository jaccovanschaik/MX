/* test.c: Description
 *
 * Copyright: (c) 2016-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2016-07-25
 * Version:   $Id: test.c 385 2017-01-16 10:25:48Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <libmx.h>

double t0;

static MX_Timer *ctrl_timer;
static MX_Timer *test_timer;

void on_test_timer(MX *mx, MX_Timer *timer, double t, void *udata)
{
    fprintf(stdout, "%s: t0 + %.1f sec.\n", __func__, t - t0);
}

void step5(MX *mx, MX_Timer *timer, double t, void *udata)
{
    fprintf(stdout, "%s: t0 + %.1f sec.\n", __func__, t - t0);

    fprintf(stdout, "%s: calling mxShutdown.\n", __func__);

    mxShutdown(mx);
}

void step4(MX *mx, MX_Timer *timer, double t, void *udata)
{
    fprintf(stdout, "%s: t0 + %.1f sec.\n", __func__, t - t0);

    /* The test timer is again 1 second in our future. Let's try to remove it
     * and make sure it doesn't time out. Then, in 2 seconds we'll shut things
     * down. */

    mxRemoveTimer(mx, timer);

    fprintf(stdout, "%s: removing test_timer.\n", __func__);

    mxRemoveTimer(mx, test_timer);

    ctrl_timer = mxCreateTimer(mx, t + 2.0, step5, NULL);

    fprintf(stdout, "%s: created new ctrl_timer, "
            "to call step5 in 1 second.\n", __func__);
}

void step3(MX *mx, MX_Timer *timer, double t, void *udata)
{
    fprintf(stdout, "%s: t0 + %.1f sec.\n", __func__, t - t0);

    /* Let's create another test timer for 2 seconds in the future, and let's
     * try to remove it in 1 seconds. */

    mxRemoveTimer(mx, timer);

    fprintf(stdout, "%s: shifting test_timer forward "
            "another 2 seconds from now.\n", __func__);

    mxAdjustTimer(mx, test_timer, t + 2.0);

    ctrl_timer = mxCreateTimer(mx, t + 1.0, step4, NULL);

    fprintf(stdout, "%s: created new ctrl_timer, "
            "to call step4 in 1 second.\n", __func__);
}

void step2(MX *mx, MX_Timer *timer, double t, void *udata)
{
    fprintf(stdout, "%s: t0 + %.1f sec.\n", __func__, t - t0);

    /* The test timer is still 1 second in our future. Adjust it to 2 seconds in
     * the future and allow it to time out. Then move on to step3. */

    mxRemoveTimer(mx, timer);

    fprintf(stdout, "%s: shifting test_timer forward "
            "to 2 seconds from now.\n", __func__);

    mxAdjustTimer(mx, test_timer, t + 2.0);

    ctrl_timer = mxCreateTimer(mx, t + 3.0, step3, NULL);

    fprintf(stdout, "%s: created new ctrl_timer, "
            "to call step3 in 3 seconds.\n", __func__);
}

void step1(MX *mx, MX_Timer *timer, double t, void *udata)
{
    fprintf(stdout, "%s: t0 + %.1f sec.\n", __func__, t - t0);

    /* Let's start a test timer for 2 seconds from now. In 1 second we'll try
     * and adjust it. */

    mxRemoveTimer(mx, timer);

    test_timer = mxCreateTimer(mx, t + 2.0, on_test_timer, NULL);

    fprintf(stdout, "%s: created new test_timer, "
            "to call on_test_timer in 2 seconds.\n",
            __func__);

    ctrl_timer = mxCreateTimer(mx, t + 1.0, step2, NULL);

    fprintf(stdout, "%s: created new ctrl_timer, "
            "to call step2 in 1 second.\n",
            __func__);
}

int main(int argc, char *argv[])
{
    MX *mx = mxClient("localhost", NULL, "Test");

    if (mx == NULL) {
        fprintf(stdout, "Error: %s", mxError());
        return -1;
    }

    t0 = mxNow();

    ctrl_timer = mxCreateTimer(mx, mxNow() + 1.0, step1, NULL);

    fprintf(stdout,
            "%s: created ctrl_timer, calling step1 in 1 second.\n",
            __func__);

    int r = mxRun(mx);

    mxDestroy(mx);

    return r;
}
