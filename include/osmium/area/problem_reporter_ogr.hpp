#ifndef OSMIUM_AREA_PROBLEM_REPORTER_OGR_HPP
#define OSMIUM_AREA_PROBLEM_REPORTER_OGR_HPP

/*

This file is part of Osmium (http://osmcode.org/libosmium).

Copyright 2013-2017 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

/**
 * @file
 *
 * This file contains code for reporting problems through OGR when
 * assembling multipolygons.
 *
 * @attention If you include this file, you'll need to link with `libgdal`.
 */

#include <osmium/area/problem_reporter.hpp>
#include <osmium/geom/factory.hpp>
#include <osmium/geom/ogr.hpp>
#include <osmium/osm/item_type.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/node_ref.hpp>
#include <osmium/osm/node_ref_list.hpp>
#include <osmium/osm/types.hpp>
#include <osmium/osm/way.hpp>

#include <gdalcpp.hpp>

#include <memory>

namespace osmium {

    namespace area {

        /**
         * Report problems when assembling areas by adding them to
         * layers in an OGR datasource.
         */
        class ProblemReporterOGR : public ProblemReporter {

            osmium::geom::OGRFactory<> m_ogr_factory;

            gdalcpp::Layer m_layer_perror;
            gdalcpp::Layer m_layer_lerror;
            gdalcpp::Layer m_layer_ways;

            void set_object(gdalcpp::Feature& feature) {
                const char t[2] = {osmium::item_type_to_char(m_object_type), '\0'};
                feature.set_field("obj_type", t);
                feature.set_field("obj_id", int32_t(m_object_id));
                feature.set_field("nodes", int32_t(m_nodes));
            }

            void write_point(const char* problem_type, osmium::object_id_type id1, osmium::object_id_type id2, osmium::Location location) {
                gdalcpp::Feature feature{m_layer_perror, m_ogr_factory.create_point(location)};
                set_object(feature);
                feature.set_field("id1", double(id1));
                feature.set_field("id2", double(id2));
                feature.set_field("problem", problem_type);
                feature.add_to_layer();
            }

            void write_line(const char* problem_type, osmium::object_id_type id1, osmium::object_id_type id2, osmium::Location loc1, osmium::Location loc2) {
                auto ogr_linestring = std::unique_ptr<OGRLineString>{new OGRLineString{}};
                ogr_linestring->addPoint(loc1.lon(), loc1.lat());
                ogr_linestring->addPoint(loc2.lon(), loc2.lat());

                gdalcpp::Feature feature{m_layer_lerror, std::move(ogr_linestring)};
                set_object(feature);
                feature.set_field("id1", static_cast<double>(id1));
                feature.set_field("id2", static_cast<double>(id2));
                feature.set_field("problem", problem_type);
                feature.add_to_layer();
            }

        public:

            explicit ProblemReporterOGR(gdalcpp::Dataset& dataset) :
                m_layer_perror(dataset, "perrors", wkbPoint),
                m_layer_lerror(dataset, "lerrors", wkbLineString),
                m_layer_ways(dataset, "ways", wkbLineString) {

                // 64bit integers are not supported in GDAL < 2, so we
                // are using a workaround here in fields where we expect
                // node IDs, we use real numbers.
                m_layer_perror
                    .add_field("obj_type", OFTString, 1)
                    .add_field("obj_id", OFTInteger, 10)
                    .add_field("nodes", OFTInteger, 8)
                    .add_field("id1", OFTReal, 12, 1)
                    .add_field("id2", OFTReal, 12, 1)
                    .add_field("problem", OFTString, 30)
                ;

                m_layer_lerror
                    .add_field("obj_type", OFTString, 1)
                    .add_field("obj_id", OFTInteger, 10)
                    .add_field("nodes", OFTInteger, 8)
                    .add_field("id1", OFTReal, 12, 1)
                    .add_field("id2", OFTReal, 12, 1)
                    .add_field("problem", OFTString, 30)
                ;

                m_layer_ways
                    .add_field("obj_type", OFTString, 1)
                    .add_field("obj_id", OFTInteger, 10)
                    .add_field("way_id", OFTInteger, 10)
                    .add_field("nodes", OFTInteger, 8)
                ;
            }

            void report_duplicate_node(osmium::object_id_type node_id1, osmium::object_id_type node_id2, osmium::Location location) override {
                write_point("duplicate_node", node_id1, node_id2, location);
            }

            void report_touching_ring(osmium::object_id_type node_id, osmium::Location location) override {
                write_point("touching_ring", node_id, 0, location);
            }

            void report_intersection(osmium::object_id_type way1_id, osmium::Location way1_seg_start, osmium::Location way1_seg_end,
                                     osmium::object_id_type way2_id, osmium::Location way2_seg_start, osmium::Location way2_seg_end, osmium::Location intersection) override {
                write_point("intersection", way1_id, way2_id, intersection);
                write_line("intersection", way1_id, way2_id, way1_seg_start, way1_seg_end);
                write_line("intersection", way2_id, way1_id, way2_seg_start, way2_seg_end);
            }

            void report_duplicate_segment(const osmium::NodeRef& nr1, const osmium::NodeRef& nr2) override {
                write_line("duplicate_segment", nr1.ref(), nr2.ref(), nr1.location(), nr2.location());
            }

            void report_overlapping_segment(const osmium::NodeRef& nr1, const osmium::NodeRef& nr2) override {
                write_line("overlapping_segment", nr1.ref(), nr2.ref(), nr1.location(), nr2.location());
            }

            void report_ring_not_closed(const osmium::NodeRef& nr, const osmium::Way* way) override {
                write_point("ring_not_closed", nr.ref(), way ? way->id() : 0, nr.location());
            }

            void report_role_should_be_outer(osmium::object_id_type way_id, osmium::Location seg_start, osmium::Location seg_end) override {
                write_line("role_should_be_outer", way_id, 0, seg_start, seg_end);
            }

            void report_role_should_be_inner(osmium::object_id_type way_id, osmium::Location seg_start, osmium::Location seg_end) override {
                write_line("role_should_be_inner", way_id, 0, seg_start, seg_end);
            }

            void report_way_in_multiple_rings(const osmium::Way& way) override {
                if (way.nodes().size() < 2) {
                    return;
                }
                try {
                    gdalcpp::Feature feature{m_layer_lerror, m_ogr_factory.create_linestring(way)};
                    set_object(feature);
                    feature.set_field("id1", int32_t(way.id()));
                    feature.set_field("id2", 0);
                    feature.set_field("problem", "way_in_multiple_rings");
                    feature.add_to_layer();
                } catch (const osmium::geometry_error&) {
                    // XXX
                }
            }

            void report_inner_with_same_tags(const osmium::Way& way) override {
                if (way.nodes().size() < 2) {
                    return;
                }
                try {
                    gdalcpp::Feature feature{m_layer_lerror, m_ogr_factory.create_linestring(way)};
                    set_object(feature);
                    feature.set_field("id1", int32_t(way.id()));
                    feature.set_field("id2", 0);
                    feature.set_field("problem", "inner_with_same_tags");
                    feature.add_to_layer();
                } catch (const osmium::geometry_error&) {
                    // XXX
                }
            }

            void report_duplicate_way(const osmium::Way& way) override {
                if (way.nodes().size() < 2) {
                    return;
                }
                try {
                    gdalcpp::Feature feature{m_layer_lerror, m_ogr_factory.create_linestring(way)};
                    set_object(feature);
                    feature.set_field("id1", int32_t(way.id()));
                    feature.set_field("id2", 0);
                    feature.set_field("problem", "duplicate_way");
                    feature.add_to_layer();
                } catch (const osmium::geometry_error&) {
                    // XXX
                }
            }

            void report_way(const osmium::Way& way) override {
                if (way.nodes().empty()) {
                    return;
                }
                if (way.nodes().size() == 1) {
                    const auto& first_nr = way.nodes()[0];
                    write_point("single_node_in_way", way.id(), first_nr.ref(), first_nr.location());
                    return;
                }
                try {
                    gdalcpp::Feature feature{m_layer_ways, m_ogr_factory.create_linestring(way)};
                    set_object(feature);
                    feature.set_field("way_id", int32_t(way.id()));
                    feature.add_to_layer();
                } catch (const osmium::geometry_error&) {
                    // XXX
                }
            }

        }; // class ProblemReporterOGR

    } // namespace area

} // namespace osmium

#endif // OSMIUM_AREA_PROBLEM_REPORTER_OGR_HPP
