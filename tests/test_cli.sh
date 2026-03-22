#!/bin/bash

set -e

# Test cogctl tool
if ! command -v ./cogctl &> /dev/null
then
    echo "cogctl could not be found"
    exit 1
fi

OUTPUT=$(./cogctl status)
if [[ "$OUTPUT" != "Cognitive extensions are active" ]]; then
    echo "Unexpected output from cogctl status: $OUTPUT"
    exit 1
fi

echo "CLI tests passed!"
