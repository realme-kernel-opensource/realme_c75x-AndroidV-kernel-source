/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef _DT_BINDINGS_MMC_MT6785_H
#define _DT_BINDINGS_MMC_MT6785_H

#define MSDC_EMMC               (0)
#define MSDC_SD                 (1)
#define MSDC_SDIO               (2)

#define MSDC_CD_HIGH            (1)
#define MSDC_CD_LOW             (0)

#define MSDC0_CLKSRC_26MHZ     (0)
#define MSDC0_CLKSRC_400MHZ    (1)
#define MSDC0_CLKSRC_200MHZ    (2)
#define MSDC0_CLKSRC_156MHZ    (3)
#define MSDC0_CLKSRC_182MHZ    (4)
#define MSDC0_CLKSRC_312MHZ    (5)

#define MSDC1_CLKSRC_26MHZ     (0)
#define MSDC1_CLKSRC_208MHZ    (1)
#define MSDC1_CLKSRC_182MHZ    (2)
#define MSDC1_CLKSRC_156MHZ    (3)
#define MSDC1_CLKSRC_200MHZ    (4)

#define MSDC_SMPL_RISING        (0)
#define MSDC_SMPL_FALLING       (1)
#define MSDC_SMPL_SEPARATE      (2)

#endif /* _DT_BINDINGS_MMC_MT6768_H */
