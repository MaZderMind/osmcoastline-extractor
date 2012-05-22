#include <getopt.h>

#define OSMIUM_MAIN
#include <osmium.hpp>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

struct config_t {
    bool debug, attempt_repair;

    char *osmfile, *coastlinedb, *outfile;
    std::vector<int> simplify_levels, admin_levels;
} cfg;

#include "output-handler.hpp"
#include "osmium-handler.hpp"

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

    // Output Handling
    OutputHandler out = OutputHandler(cfg.outfile);
    
    // Multipolygon Building
    Osmium::Handler::Multipolygon handler_multipolygon(cfg.attempt_repair, callbackPass2Handler);

    // first pass
    std::cout << "Scanning for Boundaries... 1st Pass" << std::endl;
    HandlerPass1 handler_pass1(&handler_multipolygon);
    infile.read(handler_pass1);
    std::cout << "Scanning for Boundaries... 1st Pass finished" << std::endl;

    // second pass
    std::cout << "Scanning for Boundaries... 2nd Pass" << std::endl;
    HandlerPass2 handler_pass2(&handler_multipolygon, &out);
    setPass2Handler(&handler_pass2);

    infile.read(handler_pass2);
    std::cout << "Scanning for Boundaries... 2nd Pass finished" << std::endl;

    std::cout << "Building Landmass-Shapes..." << std::endl;

    return 0;
}
