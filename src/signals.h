/* @@@LICENSE
*
*      Copyright (c) 2002-2013 LG Electronics, Inc.
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
* LICENSE@@@ */

#ifndef _SIGNALS_H_
#define _SIGNALS_H_

#include <stdbool.h>
#include <luna-service2/lunaservice.h>

/** SignalsInit
 * 
 * Register storaged with luna-service as sender of several signals.
 *
 * All signals use the category /com/palm/storage
 *
 * @param lsps_                   Handle, set earlier via a call to
 *                                LSRegister, with which to register signals
 */
void SignalsInit( LSPalmService* lsps_ );

/*
 * These are defined in a .h file so test app can use 'em.... 
 */
#define MSM_CATEGORY "/storaged"

#define MSM_METHOD_AVAIL "MSMAvail"

/** SignalMSMAvailChange
 * 
 * Send a signal called "MSMAvail" with a single parameter whose key is
 * "mode-avail" and whose value is a json boolean.  This signal is a
 * notification that mass storage mode is becoming [un-]available, i.e.,
 * normally, that the USB cable is attached.
 *
 * @param lsh                     Handle, set earlier via a call to
 *                                LSRegister, with which to send signals
 * @param avail                   used to set the value of the "mode-avail"
 *                                parameter sent with the signal
 */
void SignalMSMAvailChange( LSHandle* lsh, bool avail );

#define MSM_METHOD_PROGRESS "MSMProgress"
#define MSM_MODE_CHANGE_ATTEMPTING  "attempting"
#define MSM_MODE_CHANGE_SUCCEEDED   "succeeded"
#define MSM_MODE_CHANGE_FAILED      "failed"

/** SignalMSMProgress
 *
 * Send a signal called "MSMProgress" with one parameter whose key is "stage"
 * and whose value is one of three strings, "attempting", succeeded", or
 * "failed".  "attempting" indicates to listeners that storaged will soon
 * (currently in three seconds) be unmounting the MSM partition from
 * /media/internal and that they'd better close any open files there.
 * "succeeded" and "failed" indicate that storaged was able, or not, to
 * unmount that partition.  (Note that at least in theory our fallback to the
 * MNT_FORCE flag means the "failed" message will never be used.)
 *
 * When the parameter is "succeeded", an additional parameter, a boolean whose
 * key is "forceRequired", indicates whether the MNT_FORCE flag was needed to
 * successfully unmount the MSM partition from the local filesystem prior to
 * exporting it.
 *
 * @param lsh                     Handle, set earlier via a call to
 *                                LSRegister, with which to send signals
 *
 * @param stage                    One of the strings "attempting",
 *                                 "succeeded" or "failed"
 *
 * @param forceRequired            Used to set the "forceRequired" parameter
 *                                 of the signal when stage == "succeeded",
 *                                 ignored otherwise.
 *
 * @param enterIMasq              Used to indicate which mode we are making
 *                                 progress on, be it "disk mode" or "media sync"
 */
void SignalMSMProgress( LSHandle* lsh, const char* stage, bool forceRequired );

#define MSM_METHOD_MODE "MSMEntry"


/** SignalMSMModeChange
 *
 * Send a signal called "MSMEntry" with one parameter whose key is "new-mode"
 * and whose value is one of two strings "phone" and "brick".  These tell
 * listeners that the device has completed its transition into or out of MSM,
 * that it's become a brick (mounted on a host computer and no more usable
 * than a USB thumb drive) or that it's going back to being a phone again.
 *
 * @param lsh                     Handle, set earlier via a call to
 *                                LSRegister, with which to send signals
 *
 * @param entering                used to set the value of the "new-mode"
 *                                parameter sent with the signal to either
 *                                "brick" if true or "phone" if false.
 *
 * @param enterIMasq              If present, Used to indicate which brick mode 
 *                                is being entered, be it "disk mode" or "media sync"
 */

void SignalMSMModeChange( LSHandle* lsh, bool entering );


#define MSM_METHOD_FSCKING "MSMFscking"
/** SignalMSMFscking
 *
 * Send a signal called "MSMFscking" with no parameters.  This signal tells
 * listeners that we are in the process of running fsck.  At which point
 * the UI should present the indeterminant progress screen
 *
 * @param lsh                     Handle, set earlier via a call to
 *                                LSRegister, with which to send signals
 */
void
SignalMSMFscking( LSHandle* lsh );

#define MSM_METHOD_PARTAVAIL "PartitionAvail"
/** SignalPartitionAvail
 *
 * Send a signal called "PartitionAvail" with one parameter whose key is
 * "mount_point" and whose value is the mount point w.r.t which a device has
 * just been attached or detached and a second whose key is "available" and
 * whose value is a boolean.  These tell listeners that files may have become
 * available (or unavailable) at that mountpoint.
 *
 * @param lsh                     Handle, set earlier via a call to
 *                                LSRegister, with which to send signals
 *
 * @param mountPoint              Full path of the mount point that's changing
 *
 * @param avail                   Whether the point is newly mounted or unmounted
 * @param reformatted             (private) Whether the partition was reformatted or not 
 * @param fscked                  (private) Whether fsck found a problem or not
 */

void SignalPartitionAvail( LSHandle* lsh, const char* mountPoint, bool avail,
                           bool reformatted, bool fscked);


#define MSM_METHOD_STATUS	"MSMStatus"

/** SignalMSMStatus
 *
 * Send a signal on both public as well as private bus to let the system know if we are about to enter
 * MSM mode, or exiting MSM mode.
 *
 * @param lsh                     Handle, set earlier via a call to
 *                                LSRegister, with which to send signals
 *
 * @param inMSM                   Whether we are entering MSM mode (TRUE) or
 * 								  either failed to enter or exiting MSM mode (FALSE).
 *
 */

void SignalMSMStatus( LSHandle* lsh, bool inMSM);

#endif  /* _SIGNALS_H_ */
