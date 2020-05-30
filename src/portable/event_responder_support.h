/*
 * This file is part of the FreeRTOS port to Teensy boards.
 * Copyright (c) 2020 Timo Sandmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file    event_responder_support.h
 * @brief   FreeRTOS support implementations for Teensy EventResponder
 * @author  Timo Sandmann
 * @date    20.05.2020
 */

#pragma once

#include <cstdint>


typedef struct tskTaskControlBlock* TaskHandle_t;

namespace freertos {
static constexpr uint16_t EVENT_TASK_STACK_SIZE { 512 };

extern TaskHandle_t g_event_responder_task;

void setup_event_responder();
} // namespace freertos
