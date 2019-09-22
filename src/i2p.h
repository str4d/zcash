// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_I2P_H
#define ZCASH_I2P_H

#include "scheduler.h"

void StartI2P(boost::thread_group& threadGroup, CScheduler& scheduler);
void InterruptI2P();
void StopI2P();

#endif /* ZCASH_I2P_H */
