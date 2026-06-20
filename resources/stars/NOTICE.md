# HYG Star Catalogue Notice

`scripts/generate_star_catalog.py` produces `hyg-v41-mag65.csv` as a reduced
form of the HYG v4.1 star catalogue, filtered to stars with apparent magnitude
6.5 or brighter. The required source is pinned to commit
`c7f7f883fe678cc7680169a50ccd7dcc49b060ce` of the
[HYG Database](https://github.com/astronexus/HYG-Database). Until that generated
file is populated, the renderer uses the small real-star fallback in
`bright-stars.csv` together with its deterministic deep-sky layer.

The catalogue data is copyright David Nash and contributors and is licensed
separately from 3bs under the Creative Commons Attribution-ShareAlike 4.0
International licence. No HYG data is covered by the 3bs AGPL licence.
