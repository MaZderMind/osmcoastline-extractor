#------------------------------------------------------------------------------
#
#  osm history splitter makefile
#
#------------------------------------------------------------------------------

CXX = g++

CXXFLAGS = -g -O3 -Wall -Wextra -pedantic
CXXFLAGS += `getconf LFS_CFLAGS`
CXXFLAGS += -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
#CXXFLAGS += -Wredundant-decls -Wdisabled-optimization
#CXXFLAGS += -Wpadded -Winline

LDFLAGS = -L/usr/local/lib -lexpat -lpthread

CXXFLAGS_GEOS    = -DOSMIUM_WITH_GEOS $(shell geos-config --cflags)
CXXFLAGS_SHAPE   = -DOSMIUM_WITH_SHPLIB $(CXXFLAGS_GEOS)
CXXFLAGS_LIBXML2 = -DOSMIUM_WITH_OUTPUT_OSM_XML $(shell xml2-config --cflags)
CXXFLAGS_OGR     = -DOSMIUM_WITH_OGR $(shell gdal-config --cflags)

LIB_GD       = -lgd -lz -lm
LIB_SQLITE   = -lsqlite3
LIB_V8       = -lv8
LIB_GEOS     = $(shell geos-config --libs)
LIB_SHAPE    = -lshp $(LIB_GEOS)
LIB_PROTOBUF = -lz -lprotobuf-lite -losmpbf
LIB_XML2     = $(shell xml2-config --libs)
LIB_OGR      = $(shell gdal-config --libs)


.PHONY: all clean install

all: osmcoastline-extractor

osmcoastline-extractor: extractor.cpp
	$(CXX) $(CXXFLAGS) $(CXXFLAGS_OGR) $(CXXFLAGS_GEOS) -o $@ $< $(LDFLAGS) $(LIB_PROTOBUF) $(LIB_OGR) $(LIB_GEOS)

install:
	install -m 755 -g root -o root -d $(DESTDIR)/usr/bin
	install -m 755 -g root -o root osmcoastline-extractor $(DESTDIR)/usr/bin/osmcoastline-extractor

clean:
	rm -f *.o core osm-history-splitter

spaces:
	dos2unix *.cpp *.hpp
	sed -i 's/\t/    /g' *.cpp *.hpp
	sed -i 's/[ \t]*$$//g' *.cpp *.hpp
