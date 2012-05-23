#include <osmium/storage/byid/sparsetable.hpp>
#include <osmium/storage/byid/mmap_file.hpp>
#include <osmium/handler/coordinates_for_ways.hpp>
#include <osmium/handler/multipolygon.hpp>
#include <osmium/geometry/multipolygon.hpp>

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

    storage_sparsetable_t store_pos;
    storage_mmap_t store_neg;
    cfw_handler_t* handler_cfw;

    Osmium::Handler::Multipolygon* handler_multipolygon;
    OutputHandler *out;

public:

    HandlerPass2(Osmium::Handler::Multipolygon* hmp, OutputHandler* output) : handler_multipolygon(hmp), out(output) {
        handler_cfw = new cfw_handler_t(store_pos, store_neg);
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
            
            // ignore bad-bad-bad, unnamed boundary-ways
            //  http://www.openstreetmap.org/api/0.6/way/23623225/4
            if (!name) {
                std::cerr << "  ignoring unnamed boundary from ";
                if (area->get_type() == 3) {
                    std::cerr << "way ";
                }
                else if (area->get_type() == 4) {
                    std::cerr << "relation ";
                }
                std::cerr << area->id() << std::endl;
                return;
            }

            out->addAdministrative(area);
        }
    }

};

HandlerPass2* hpass2;

void callbackPass2Handler(Osmium::OSM::Area* area) {
    hpass2->area(area);
}

void setPass2Handler(HandlerPass2* handler) {
    hpass2 = handler;
}
