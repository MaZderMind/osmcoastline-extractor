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

void setPass2Handler(HandlerPass2* handler) {
    hpass2 = handler;
}
