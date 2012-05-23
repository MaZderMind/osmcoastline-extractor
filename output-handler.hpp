#include <ogrsf_frmts.h>

class OutputHandler {
private:
    OGRDataSource* ds;
    OGRSpatialReference sparef;

    OGRLayer* layer_admin;
    OGRLayer* layer_adminpoint;

public:
    OutputHandler(const char *outfile) {
        OGRRegisterAll();

        // SQLite Database
        const char* driver_name = "SQLite";
        OGRSFDriver* driver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName(driver_name);
        if (driver == NULL) {
            std::cerr << driver_name << " driver not available.\n";
            exit(1);
        }

        CPLSetConfigOption("OGR_SQLITE_SYNCHRONOUS", "FALSE");
        const char* options[] = { "SPATIALITE=TRUE", NULL };
        ds = driver->CreateDataSource(outfile, const_cast<char**>(options));
        if (ds == NULL) {
            std::cerr << "Creation of output file " << cfg.outfile << " failed.\n";
            exit(1);
        }

        // Spatial Reference System
        sparef.SetWellKnownGeogCS("WGS84");

        // Layer
        init_admin_layer();
        init_adminpoint_layer();
        // TODO: init landmass layer
    }

    void init_admin_layer() {
        layer_admin = ds->CreateLayer("administrative", &sparef, wkbMultiPolygon, NULL);
        if (layer_admin == NULL) {
            std::cerr << "Creating layer failed.\n";
            exit(1);
        }

        OGRFieldDefn field_osmid("osm_id", OFTInteger);
        field_osmid.SetWidth(10);
        if (layer_admin->CreateField(&field_osmid) != OGRERR_NONE ) {
            std::cerr << "Creating field on failed.\n";
            exit(1);
        }

        OGRFieldDefn field_fromtype("from_type", OFTString);
        field_fromtype.SetWidth(1);
        if (layer_admin->CreateField(&field_fromtype) != OGRERR_NONE ) {
            std::cerr << "Creating field failed.\n";
            exit(1);
        }

        OGRFieldDefn field_area("area", OFTReal);
        if (layer_admin->CreateField(&field_area) != OGRERR_NONE ) {
            std::cerr << "Creating field failed.\n";
            exit(1);
        }

        OGRFieldDefn field_name("name", OFTString);
        field_name.SetWidth(100);
        if (layer_admin->CreateField(&field_name) != OGRERR_NONE ) {
            std::cerr << "Creating field failed.\n";
            exit(1);
        }
    }

    void init_adminpoint_layer() {
        layer_adminpoint = ds->CreateLayer("administrative_point", &sparef, wkbPoint, NULL);
        if (layer_adminpoint == NULL) {
            std::cerr << "Creating layer failed.\n";
            exit(1);
        }

        OGRFieldDefn field_osmid("osm_id", OFTInteger);
        field_osmid.SetWidth(10);
        if (layer_adminpoint->CreateField(&field_osmid) != OGRERR_NONE ) {
            std::cerr << "Creating field on failed.\n";
            exit(1);
        }

        OGRFieldDefn field_fromtype("from_type", OFTString);
        field_fromtype.SetWidth(1);
        if (layer_adminpoint->CreateField(&field_fromtype) != OGRERR_NONE ) {
            std::cerr << "Creating field failed.\n";
            exit(1);
        }

        OGRFieldDefn field_area("area", OFTReal);
        if (layer_adminpoint->CreateField(&field_area) != OGRERR_NONE ) {
            std::cerr << "Creating field failed.\n";
            exit(1);
        }

        OGRFieldDefn field_name("name", OFTString);
        field_name.SetWidth(100);
        if (layer_adminpoint->CreateField(&field_name) != OGRERR_NONE ) {
            std::cerr << "Creating field failed.\n";
            exit(1);
        }
    }


    void addAdministrative(Osmium::OSM::Area* area) {
        try {
            const char* name = area->tags().get_tag_by_key("name");
            std::cerr << "addding area for " << name << std::endl;
            
            Osmium::Geometry::MultiPolygon mp(*area);
            OGRFeature* feature = OGRFeature::CreateFeature(layer_admin->GetLayerDefn());
            OGRMultiPolygon* ogrgeom = mp.create_ogr_geometry();
            
            feature->SetGeometry(ogrgeom);
            feature->SetField("osm_id", area->id());            
            feature->SetField("area", ogrgeom->get_Area());
            if(name) feature->SetField("name", name);

            switch(area->get_type()) {
                case  AREA_FROM_WAY:
                    feature->SetField("from_type", "w");
                    break;
                case  AREA_FROM_RELATION:
                    feature->SetField("from_type", "r");
                    break;
                default:
                    feature->SetField("from_type", "?");
                    break;
            }
            
            if (layer_admin->CreateFeature(feature) != OGRERR_NONE) {
                std::cerr << "Failed to create feature.\n";
                exit(1);
            }

            OGRFeature::DestroyFeature(feature);
            
            // store PointOnSurface
            for (int i = 0; i < ogrgeom->getNumGeometries(); i++) {
                OGRGeometry* subgeom = ogrgeom->getGeometryRef(i);
                OGRPolygon *poly = dynamic_cast<OGRPolygon*>(subgeom);
                
                if (poly) {
                    OGRPoint *point = new OGRPoint();
                    if (poly->PointOnSurface(point) != OGRERR_NONE) {
                        std::cerr << "Failed to calculate PointOnSurface.\n";
                        if(point) delete point;
                        continue;
                    }
                
                    OGRFeature* subfeature = OGRFeature::CreateFeature(layer_adminpoint->GetLayerDefn());
                    subfeature->SetGeometry(point);
                    subfeature->SetField("osm_id", area->id());            
                    subfeature->SetField("area", poly->get_Area());
                    if(name) subfeature->SetField("name", name);

                    switch(area->get_type()) {
                        case  AREA_FROM_WAY:
                            subfeature->SetField("from_type", "w");
                            break;
                        case  AREA_FROM_RELATION:
                            subfeature->SetField("from_type", "r");
                            break;
                        default:
                            subfeature->SetField("from_type", "?");
                            break;
                    }
                    
                    if (layer_adminpoint->CreateFeature(subfeature) != OGRERR_NONE) {
                        std::cerr << "Failed to create feature.\n";
                        exit(1);
                    }

                    OGRFeature::DestroyFeature(subfeature);
                    delete point;
                }
            }
            
            delete ogrgeom;
        } catch (Osmium::Exception::IllegalGeometry) {
            std::cerr << "Ignoring illegal geometry for multipolygon " << area->id() << ".\n";
        }
    }
};
