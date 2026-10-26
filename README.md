# factory

Evolves small Factorio layouts with a genetic algorithm. The first target: maximise iron
gears/s in a 10×10 area, from an infinite iron-plate chest at (0,4) into an output chest at
(9,4), using Assembling machine 1, yellow inserters and yellow belts (tier 1, no power).

## Build

    cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release -j

Tests (Debug, with sanitizers):

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j && ctest --test-dir build

## Use

    ./build-release/factory run [--pop 200] [--gens 500] [--seed 42] [--threads N] [--out best] [--seeds]
    ./build-release/factory sim best.json
    ./build-release/factory blueprint best.json

`run` prints progress and the best layout, then writes `<out>.json` (Factorio-style entity
list) and `<out>.svg`. By default the GA starts from random layouts only; `--seeds`
also starts ~10% of the population from a hand-made working layout. `sim` re-simulates a saved or hand-written layout.

## Try it in Factorio (2.0)

`run` also writes `<out>.blueprint.txt`; `factory blueprint <layout.json>` prints the same
string for any saved layout. In a sandbox or editor world (`/editor`), open the blueprint
library → *Import string*, paste, and place it. The blueprint is self-contained for testing:
the input chest is an infinity chest stocked with iron plates, the output is a wooden chest,
assemblers come with the gear recipe, and two substations with electric energy interfaces
just outside the 10×10 power everything. Expect slightly under the simulated rate: the
simulator treats inserters picking from belts as fast as from chests.

ASCII legend: `I` input chest, `O` output chest, `A` assembler, `^ > v <` belts,
`n e s w` inserters (letter = drop direction).

The theoretical ceiling is ~1.25 gears/s: an edge chest has only 3 in-grid neighbours, so
at most 3 inserters × 0.83 plates/s can leave it.
