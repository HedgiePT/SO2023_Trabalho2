#!/bin/bash

rm -f error*
rm -f core

# change 0x611a48c7 to your semaphore and shared memory key
ipcrm -S 0x611a48c7
ipcrm -M 0x611a48c7
