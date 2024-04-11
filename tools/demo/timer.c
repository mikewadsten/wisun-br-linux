/*
 * Copyright (c) 2024 Silicon Laboratories Inc. (www.silabs.com)
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at [1].  This software is distributed to you in
 * Object Code format and/or Source Code format and is governed by the sections
 * of the MSLA applicable to Object Code, Source Code and Modified Open Source
 * Code. By using this software, you agree to the terms of the MSLA.
 *
 * [1]: https://www.silabs.com/about-us/legal/master-software-license-agreement
 */
#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <poll.h>

#include "common/log.h"
#include "common/memutils.h"
#include "common/timer.h"

struct module {
    uint64_t delay_ms;
    struct timer_group timer_group;
};

void timer_cb(struct timer_group *group, struct timer_entry *timer)
{
    printf("%s() %"PRIu64"ms\n", __func__, timer->period_ms);
}

void timer_cb_exp(struct timer_group *group, struct timer_entry *timer)
{
    struct module *mod = container_of(group, struct module, timer_group);

    printf("%s() %"PRIu64"ms\n", __func__, mod->delay_ms);
    timer_start_rel(group, timer, mod->delay_ms *= 2);
}

void timer_cb_rand(struct timer_group *group, struct timer_entry *timer)
{
    uint64_t offset_ms = ((double)rand() / RAND_MAX) * 5000;

    printf("%s() next in %"PRIu64"ms\n", __func__, offset_ms);
    timer_start_rel(group, timer, offset_ms);
}

int main()
{
    struct timer_ctxt ctxt = { };
    struct module mod = {
        .delay_ms = 1,
    };
    struct pollfd pfd = { };
    struct timer_entry timer_500ms = {
        .period_ms = 500,
        .callback = timer_cb,
    };
    struct timer_entry timer_666ms = {
        .period_ms = 666,
        .callback = timer_cb,
    };
    struct timer_entry timer_exp = {
        .callback = timer_cb_exp,
    };
    struct timer_entry timer_rand = {
        .callback = timer_cb_rand,
    };
    int ret;

    srand(0);

    timer_ctxt_init(&ctxt);
    pfd.fd = ctxt.fd;
    pfd.events = POLLIN;

    timer_group_init(&ctxt, &mod.timer_group);

    timer_start_rel(&mod.timer_group, &timer_500ms, timer_500ms.period_ms);
    timer_start_rel(&mod.timer_group, &timer_666ms, timer_666ms.period_ms);
    timer_start_rel(&mod.timer_group, &timer_exp,   0);
    timer_start_rel(&mod.timer_group, &timer_rand,  0);

    while (1) {
        ret = poll(&pfd, 1, -1);
        FATAL_ON(ret < 0, 2, "poll: %m");
        if (pfd.revents & POLLIN)
            timer_ctxt_process(&ctxt);
    }
}
