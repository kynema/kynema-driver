# ExaWind Simulation Driver
[![journal article](https://img.shields.io/badge/DOI-10.1002/we.2886-blue)](https://doi.org/10.1002/we.2886)

This repository contains a pure C++ driver API for coupling
[Kynema-UGF](https://github.com/exawind/nalu-wind) and
[Kynema-SGF](https://github.com/exawind/amr-wind) CFD codes.

## Citation

To cite ExaWind or the usage of this driver and to learn more about the methodology, use the following [journal article](https://doi.org/10.1002/we.2886):
```
@article{https://doi.org/10.1002/we.2886,
    author = {Sharma, Ashesh and Brazell, Michael J. and Vijayakumar, Ganesh and Ananthan, Shreyas and Cheung, Lawrence and deVelder, Nathaniel and {Henry de Frahan}, Marc T. and Matula, Neil and Mullowney, Paul and Rood, Jon and Sakievich, Philip and Almgren, Ann and Crozier, Paul S. and Sprague, Michael},
    title = {ExaWind: Open-source CFD for hybrid-RANS/LES geometry-resolved wind turbine simulations in atmospheric flows},
    journal = {Wind Energy},
    volume = {27},
    number = {3},
    pages = {225-257},
    doi = {https://doi.org/10.1002/we.2886},
    url = {https://onlinelibrary.wiley.com/doi/abs/10.1002/we.2886},
    eprint = {https://onlinelibrary.wiley.com/doi/pdf/10.1002/we.2886},
    year = {2024}
}
```

## Contributing

To pass the format check use this with a new version of `clang-format`:
```
find ./app ./src \( -name "*.cpp" -o -name "*.H" -o -name "*.h" -o -name "*.C" \) -exec clang-format -i {} +
```
