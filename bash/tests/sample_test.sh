#!/bin/bash

/bin/bash -c "ls /tmp"

STATUS=$?

echo "EXIT_STATUS=${STATUS}"

exit ${STATUS}
