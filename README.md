osmcoastline-extractor
======================

Extracts smaller shapes from an osmcoastline-Database, created using
Jochen Topf's ocmcoastline-tool: https://github.com/joto/osmcoastline

Feed it a spatialite-Database  as COASTLINE generated with above
mentioned tool, the used planet-file as OSMFILE and the name
of the output shapelib as OUTFILE.
The extractor will calculate a shape for all matched admin-levels,
intersect them with the coastline in the coastline-Database
thus generating another shape which represents the landmass
for this boundary.

The two shapes are simplified using the specified simplification
parameters and written to a combined spatialite database, together
with the osm-id and the name of the admin-boundary.
