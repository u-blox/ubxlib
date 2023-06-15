/******************************************************************************
 * Copyright 2013-2023 u-blox AG, Thalwil, Switzerland
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************
 *
 * Project: libMGA
 * Purpose: Library providing functions to help a host application to download
 *          MGA assistance data and pass it on to a u-blox GNSS receiver.
 *
 *****************************************************************************/

// MODIFIED: gate changed to match renamed file
#ifndef __U_LIB_MGA_COMMON_TYPES__
#define __U_LIB_MGA_COMMON_TYPES__  //!< multiple inclusion guard

//! Standard u-blox types
/*! These internal standard types are defined here with UBX_ prepended in order to avoid
    conflicts when integrating with other languages.
*/
typedef signed char            UBX_I1;  //!< signed 1 byte integer
typedef signed short           UBX_I2;  //!< signed 2 byte integer
typedef signed int             UBX_I4;  //!< signed 4 byte integer
typedef signed long long int   UBX_I8;  //!< signed 8 byte integer
typedef unsigned char          UBX_U1;  //!< unsigned 1 byte integer
typedef unsigned short         UBX_U2;  //!< unsigned 2 byte integer
typedef unsigned int           UBX_U4;  //!< unsigned 4 byte integer
typedef unsigned long long int UBX_U8;  //!< unsigned 8 byte integer
// MODIFIED: we don't use floating point inside ubxlib, not required
#if 0
typedef float                  UBX_R4;  //!< 4 byte floating point
typedef double                 UBX_R8;  //!< 8 byte floating point
#endif // Not required
typedef char                   UBX_CH;  //!< ASCII character

#endif //__U_LIB_MGA_COMMON_TYPES__

//@}
