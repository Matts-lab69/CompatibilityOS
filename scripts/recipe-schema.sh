#!/usr/bin/env bash
# SPDX-License-Identifier: MIT

read_recipe() {
    local recipe=$1 license line number expected=1 format kind first
    recipe_migrations=()
    [[ -f $recipe/recipe.conf && ! -L $recipe && ! -L $recipe/recipe.conf ]] || return 1
    {
        IFS= read -r license && [[ $license == '# SPDX-License-Identifier: MIT' ]] || return 1
        IFS= read -r format && [[ $format == 'format=1' ]] || return 1
        IFS= read -r kind && [[ $kind == 'kind=exe' || $kind == 'kind=msi' ]] || return 1
        while IFS= read -r line || [[ -n $line ]]; do
            [[ $line =~ ^migration=([0-9]{4})\.reg$ ]] || return 1
            number=${BASH_REMATCH[1]}
            (( 10#$number == expected )) || return 1
            [[ -f $recipe/$number.reg && ! -L $recipe/$number.reg ]] || return 1
            IFS= read -r first < "$recipe/$number.reg" || return 1
            [[ $first == 'Windows Registry Editor Version 5.00' ]] || return 1
            recipe_migrations+=("$recipe/$number.reg")
            expected=$((expected + 1))
        done
    } < "$recipe/recipe.conf"
}
