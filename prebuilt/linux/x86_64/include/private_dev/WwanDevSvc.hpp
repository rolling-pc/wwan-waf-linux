/**
 * Copyright (C) 2024 Rolling Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 * 
 * @file wwan_func_svc.hpp
 * @author xxx@rolling.com (xxx)
 * @brief
 * @version 1.0
 * @date 2024-08-xx
 *
 **/

#ifndef WWAN_DEV_SVC_HPP
#define WWAN_DEV_SVC_HPP

namespace WwanDevSvc
{
    void startWwanDevServiceWork();
    void stopWwanDevServiceWork();
    void onPowerEvent(unsigned eventType);
};

#endif // WWANFUNCSVC_H