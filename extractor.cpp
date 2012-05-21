#include <getopt.h>

#define OSMIUM_MAIN
#include <osmium.hpp>
#include <osmium/storage/byid/sparsetable.hpp>
#include <osmium/storage/byid/mmap_file.hpp>
#include <osmium/handler/coordinates_for_ways.hpp>
#include <osmium/handler/multipolygon.hpp>
#include <osmium/geometry/multipolygon.hpp>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

struct tconfig {
    bool debug, attempt_repair;

    char *osmfile, *coastlinedb, *outfile;
    std::vector<int> simplify_levels, admin_levels;
} cfg;

typedef Osmium::Storage::ById::SparseTable<Osmium::OSM::Position> storage_sparsetable_t;
typedef Osmium::Storage::ById::MmapFile<Osmium::OSM::Position> storage_mmap_t;
typedef Osmium::Handler::CoordinatesForWays<storage_sparsetable_t, storage_mmap_t> cfw_handler_t;

class HandlerPass1 : public Osmium::Handler::Base {

    Osmium::Handler::Multipolygon* handler_multipolygon;

public:

    HandlerPass1(Osmium::Handler::Multipolygon* hmp) : handler_multipolygon(hmp) {
    }

    ~HandlerPass1() {
    }

    void before_relations() {
        handler_multipolygon->before_relations();
    }

    void relation(const shared_ptr<Osmium::OSM::Relation const>& relation) {
        handler_multipolygon->relation(relation);
    }

    void after_relations() {
        handler_multipolygon->after_relations();
    }

};

/* ================================================== */

class HandlerPass2 : public Osmium::Handler::Base {

    #if 0
    OGRDataSource* m_data_source;
    OGRLayer* m_layer_mp;
    #endif

    storage_sparsetable_t store_pos;
    storage_mmap_t store_neg;
    cfw_handler_t* handler_cfw;

    Osmium::Handler::Multipolygon* handler_multipolygon;

public:

    HandlerPass2(Osmium::Handler::Multipolygon* hmp) : handler_multipolygon(hmp) {
        handler_cfw = new cfw_handler_t(store_pos, store_neg);

        #if 0
        OGRRegisterAll();

        const char* driver_name = "SQLite";
        OGRSFDriver* driver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName(driver_name);
        if (driver == NULL) {
            std::cerr << driver_name << " driver not available.\n";
            exit(1);
        }

        CPLSetConfigOption("OGR_SQLITE_SYNCHRONOUS", "FALSE");
        const char* options[] = { "SPATIALITE=TRUE", NULL };
        m_data_source = driver->CreateDataSource("ogr_out.sqlite", const_cast<char**>(options));
        if (m_data_source == NULL) {
            std::cerr << "Creation of output file failed.\n";
            exit(1);
        }

        OGRSpatialReference sparef;
        sparef.SetWellKnownGeogCS("WGS84");
        m_layer_mp = m_data_source->CreateLayer("areas", &sparef, wkbMultiPolygon, NULL);
        if (m_layer_mp == NULL) {
            std::cerr << "Layer creation failed.\n";
            exit(1);
        }

        OGRFieldDefn layer_mp_field_id("id", OFTInteger);
        layer_mp_field_id.SetWidth(10);

        if (m_layer_mp->CreateField(&layer_mp_field_id) != OGRERR_NONE ) {
            std::cerr << "Creating id field failed.\n";
            exit(1);
        }
        #endif
    }

    ~HandlerPass2() {
        #if 0
        OGRDataSource::DestroyDataSource(m_data_source);
        #endif
        delete handler_cfw;
    }

    void init(Osmium::OSM::Meta& meta) {
        handler_cfw->init(meta);
    }

    void node(const shared_ptr<Osmium::OSM::Node const>& node) {
        handler_cfw->node(node);
    }

    void after_nodes() {
        handler_cfw->after_nodes();
    }

    void way(const shared_ptr<Osmium::OSM::Way>& way) {
        handler_cfw->way(way);
        handler_multipolygon->way(way);
    }

    void area(Osmium::OSM::Area* area) {
        const char* boundary = area->tags().get_tag_by_key("boundary");
        const char* admin_level = area->tags().get_tag_by_key("admin_level");

        if (boundary && admin_level && strcmp("administrative", boundary) == 0) {
            if(std::find(cfg.admin_levels.begin(), cfg.admin_levels.end(), atoi(admin_level)) == cfg.admin_levels.end()) {
                return;
            }

            const char* name = area->tags().get_tag_by_key("name");
            
            // ignore bad, unnamed, boundary-ways
            //  http://www.openstreetmap.org/api/0.6/way/23623225/4
            if (!name) {
                std::cerr << "  ignoring unnamed boundary from ";
                if (area->get_type() == 3) {
                    std::cerr << "way ";
                }
                else if (area->get_type() == 4) {
                    std::cerr << " relation";
                }
                std::cerr << area->id() << std::endl;
                return;
            }

            std::cout << "Boundary level " << admin_level << ": " << name << " (" << "#" << area->id() << ")" << std::endl;
        }

        #if 0
        const char* building = area->tags().get_tag_by_key("building");
        if (building) {
            try {
                Osmium::Geometry::MultiPolygon mp(*area);

                OGRFeature* feature = OGRFeature::CreateFeature(m_layer_mp->GetLayerDefn());
                OGRMultiPolygon* ogrmp = mp.create_ogr_geometry();
                feature->SetGeometry(ogrmp);
                feature->SetField("id", area->id());

                if (m_layer_mp->CreateFeature(feature) != OGRERR_NONE) {
                    std::cerr << "Failed to create feature.\n";
                    exit(1);
                }

                OGRFeature::DestroyFeature(feature);
                delete ogrmp;
            } catch (Osmium::Exception::IllegalGeometry) {
                std::cerr << "Ignoring illegal geometry for multipolygon " << area->id() << ".\n";
            }
        }
        #endif
    }

};

HandlerPass2* hpass2;

void callbackPass2Handler(Osmium::OSM::Area* area) {
    hpass2->area(area);
}

void usage(char* binary) {
    std::cout <<
        binary << " [OPTIONS] COASTLINE OSMFILE OUTFILE" << std::endl <<
        std::endl <<
        "Extracts smaller shapes from an osmcoastline-Database, created using" << std::endl <<
        "Jochen Topf's ocmcoastline-tool: https://github.com/joto/osmcoastline" << std::endl <<
        std::endl <<
        "Feed it a spatialite-Database  as COASTLINE generated with above" << std::endl <<
        "mentioned tool, the used planet-file as OSMFILE and the name" << std::endl <<
        "of the output shapelib as OUTFILE." << std::endl <<
        "The extractor will calculate a shape for all matched admin-levels," << std::endl <<
        "intersect them with the coastline in the coastline-Database" << std::endl <<
        "thus generating another shape which represents the landmass" << std::endl <<
        "for this boundary." << std::endl <<
        std::endl <<
        "The two shapes are simplified using the specified simplification" << std::endl <<
        "parameters and written to a combined spatialite database, together" << std::endl <<
        "with the osm-id and the name of the admin-boundary." << std::endl;

        #if 0
        "Both shapes will be simplified in three levels and projected" << std::endl <<
        "into both Wgs84 as well Spherical Mercator. All resulting shapes" << std::endl <<
        "are stored as shapefiles - one per boundary. Also a combined" << std::endl <<
        "shapefile and a spatialite-database is generated with the" << std::endl <<
        "osm_id and the name of the boundary as a column." << std::endl;
        #endif
}

void split_intlist(std::vector<int> *arr, char* list) {
    std::vector<std::string> split_vector;
    boost::algorithm::split(split_vector, list, boost::algorithm::is_any_of(","), boost::algorithm::token_compress_on);
    if(split_vector.size() > 0) {
        arr->clear();
    }
    for (std::vector<std::string>::iterator it = split_vector.begin() ; it < split_vector.end(); it++) {
        arr->push_back(atoi( it->c_str() ));
    }
}

int main(int argc, char *argv[]) {
    cfg.debug = false;
    cfg.attempt_repair = true;

    cfg.admin_levels.push_back(2);
    cfg.simplify_levels.push_back(0);
    cfg.simplify_levels.push_back(100);
    cfg.simplify_levels.push_back(10000);

    static struct option long_options[] = {
        {"debug",                  no_argument, 0, 'd'},
        {"no-attempt_repair",      no_argument, 0, 'R'},

        {"simplify-levels",        required_argument, 0, 'l'},
        {"admin-levels",           required_argument, 0, 'a'},

        {0, 0, 0, 0}
    };

    while (1) {
        int c = getopt_long(argc, argv, "dla", long_options, 0);
        if (c == -1)
            break;

        switch (c) {
            case 'd': {
                cfg.debug = true;
                break;
            }
            case 'R': {
                cfg.attempt_repair = false;
                break;
            }

            case 'l': {
                split_intlist(&cfg.simplify_levels, optarg);
                break;
            }
            case 'a': {
                split_intlist(&cfg.admin_levels, optarg);
                break;
            }
        }
    }

    if (optind != argc-3) {
        usage(argv[0]);
        return 1;
    }

    cfg.coastlinedb = argv[optind+0];
    cfg.osmfile = argv[optind+1];
    cfg.outfile = argv[optind+2];

    std::cout <<
        "  Extracting boundaries from: " << cfg.osmfile << std::endl <<
        "  Matching against Coastlines from: " << cfg.coastlinedb << std::endl <<
        "  Writing results to: " << cfg.outfile << std::endl;

    Osmium::init(cfg.debug);
    Osmium::OSMFile infile(cfg.osmfile);

    Osmium::Handler::Multipolygon handler_multipolygon(cfg.attempt_repair, callbackPass2Handler);

    // first pass
    std::cout << "Scanning for Boundaries... 1st Pass" << std::endl;
    HandlerPass1 handler_pass1(&handler_multipolygon);
    infile.read(handler_pass1);
    std::cout << "Scanning for Boundaries... 1st Pass finished" << std::endl;

    // second pass
    std::cout << "Scanning for Boundaries... 2nd Pass" << std::endl;
    HandlerPass2 handler_pass2(&handler_multipolygon);
    hpass2 = &handler_pass2;

    infile.read(handler_pass2);
    std::cout << "Scanning for Boundaries... 2nd Pass finished" << std::endl;

    std::cout << "Building Landmass-Shapes..." << std::endl;

    return 0;
}
