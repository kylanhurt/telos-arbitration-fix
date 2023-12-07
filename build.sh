#! /bin/bash

#contract
# if [[ "$1" == "arbitration" ]]; then
#     contract=arbitration
# else
#     echo "need contract"
#     exit 0
# fi

echo ">>> Building arbitration contract..."

# eosio.cdt v1.7
# -contract=<string>       - Contract name
# -o=<string>              - Write output to <file>
# -abigen                  - Generate ABI
# -I=<string>              - Add directory to include search path
# -L=<string>              - Add directory to library search path
# -R=<string>              - Add a resource path for inclusion

eosio-cpp -I="./contracts/arbitration/include/" -R="./contracts/arbitration/resources" -o="./build/arbitration.wasm" -contract="arbitration" -abigen ./contracts/arbitration/src/arbitration.cpp
