#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
root=$(cd -- "${BASH_SOURCE[0]%/*}/.." && pwd -P)
# shellcheck source=scripts/recipe-schema.sh
source "$root/scripts/recipe-schema.sh"
count=0
for recipe in "$root/recipes/"*/; do
    recipe=${recipe%/}
    [[ ${recipe##*/} =~ ^[a-z0-9][a-z0-9-]{0,62}$ ]]
    read_recipe "$recipe" || { printf 'Invalid recipe: %s\n' "$recipe" >&2; exit 1; }
    (( ${#recipe_migrations[@]} > 0 ))
    count=$((count + 1))
done
printf 'Recipes: %d verified.\n' "$count"
