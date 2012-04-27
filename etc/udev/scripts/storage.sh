# @@@LICENSE
#
#      Copyright (c) 2002-2012 Hewlett-Packard Development Company, L.P.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# LICENSE@@@

DbgPrint() {
    #echo "$@" > /dev/null
    logger -t udev_storaged -p debug "$@"
}

ErrPrint() {
    logger -t udev_storaged -p err "$@"
}

DbgPrint "$0($1,$2) called: "

action="$1"
change="$2"

if [ "$action" == "HOST_STATE_CHANGED" ]; then
    method="changed"
    change_name="connected"
elif [ "$action" == "MEDIA_STATE_CHANGED" ]; then
    method="avail"
    change_name="connected"
elif [ "$action" == "MEDIA_REQUEST_STATE_CHANGED" ]; then
    method="requestMedia"
    change_name="connected"
elif [ "$action" == "BUS_STATE_CHANGED" ]; then
    method="busSuspended"
    change_name="suspended"
else
    ErrPrint "unknown action $action passed to $0"
    return
fi

if [ "$change" == "0" ]; then
    MESSAGE="{\"$change_name\": false}"
elif [ "$change" == "1" ]; then
    MESSAGE="{\"$change_name\": true}"
else
    ErrPrint "unexpected change $change passed to $0"
    return
fi

# TODO: use luna-helper rather than luna-send here
DbgPrint "/usr/bin/luna-send -n 1 luna://com.palm.storage/diskmode/$method \"$MESSAGE\""
/usr/bin/luna-send -n 1 luna://com.palm.storage/diskmode/$method "$MESSAGE"
