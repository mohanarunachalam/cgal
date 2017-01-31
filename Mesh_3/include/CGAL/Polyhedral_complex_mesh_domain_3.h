// Copyright (c) 2009-2010 INRIA Sophia-Antipolis (France).
// Copyright (c) 2014-2017 GeometryFactory Sarl (France)
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
// You can redistribute it and/or modify it under the terms of the GNU
// General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $URL$
// $Id$
//
//
// Author(s)     : Laurent Rineau

#ifndef CGAL_POLYHEDRAL_COMPLEX_MESH_DOMAIN_3_H
#define CGAL_POLYHEDRAL_COMPLEX_MESH_DOMAIN_3_H

#include <CGAL/Mesh_3/config.h>

#include <CGAL/Random.h>
#include <CGAL/Polyhedral_mesh_domain_3.h>
#include <CGAL/Mesh_domain_with_polyline_features_3.h>
#include <CGAL/Mesh_polyhedron_3.h>

#include <CGAL/Polygon_mesh_processing/compute_normal.h>

#include <CGAL/Mesh_3/Polyline_with_context.h>
#include <CGAL/Mesh_3/Detect_features_in_polyhedra.h>

#include <CGAL/enum.h>
#include <CGAL/IO/Polyhedron_iostream.h>
#include <CGAL/boost/graph/graph_traits_Polyhedron_3.h>
#include <CGAL/boost/graph/helpers.h>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/dynamic_bitset.hpp>
#include <CGAL/boost/graph/split_graph_into_polylines.h>

#include <boost/iterator/transform_iterator.hpp>
#include <boost/foreach.hpp>

#include <string>
#include <vector>
#include <fstream>


namespace CGAL {

namespace internal {
namespace Mesh_3 {

template <typename Kernel>
struct Angle_tester
{
  template <typename vertex_descriptor, typename Graph>
  bool operator()(vertex_descriptor& v, const Graph& g) const
  {
    typedef typename boost::graph_traits<Graph>::out_edge_iterator out_edge_iterator;
    if (out_degree(v, g) != 2)
      return true;
    else
    {
      out_edge_iterator out_edge_it, out_edges_end;
      boost::tie(out_edge_it, out_edges_end) = out_edges(v, g);

      vertex_descriptor v1 = target(*out_edge_it++, g);
      vertex_descriptor v2 = target(*out_edge_it++, g);
      CGAL_assertion(out_edge_it == out_edges_end);

      const typename Kernel::Point_3& p = g[v];
      const typename Kernel::Point_3& p1 = g[v1];
      const typename Kernel::Point_3& p2 = g[v2];

      return (CGAL::angle(p1, p, p2) == CGAL::ACUTE);
    }
  }
};

template <typename Polyhedron>
struct Is_featured_edge {
  const Polyhedron* polyhedron;
  Is_featured_edge() : polyhedron(0) {} // required by boost::filtered_graph
  Is_featured_edge(const Polyhedron& polyhedron) : polyhedron(&polyhedron) {}

  bool operator()(typename boost::graph_traits<Polyhedron>::edge_descriptor e) const {
    return halfedge(e, *polyhedron)->is_feature_edge();
  }
}; // end Is_featured_edge<Polyhedron>

template <typename Polyhedron>
struct Is_border_edge {
  const Polyhedron* polyhedron;
  Is_border_edge() : polyhedron(0) {} // required by boost::filtered_graph
  Is_border_edge(const Polyhedron& polyhedron) : polyhedron(&polyhedron) {}

  bool operator()(typename boost::graph_traits<Polyhedron>::edge_descriptor e) const {
    return is_border(halfedge(e, *polyhedron), *polyhedron) ||
      is_border(opposite(halfedge(e, *polyhedron), *polyhedron), *polyhedron);
  }
}; // end Is_featured_edge<Polyhedron>

template<typename Polyhedral_mesh_domain,
         typename Polyline_with_context,
         typename Graph>
struct Extract_polyline_with_context_visitor
{
  typedef typename Polyhedral_mesh_domain::Polyhedron Polyhedron;
  std::vector<Polyline_with_context>& polylines;
  const Graph& graph;

  Extract_polyline_with_context_visitor
  (const Graph& graph,
   typename std::vector<Polyline_with_context>& polylines)
    : polylines(polylines), graph(graph)
  {}

  void start_new_polyline()
  {
    polylines.push_back(Polyline_with_context());
  }

  void add_node(typename boost::graph_traits<Graph>::vertex_descriptor vd)
  {
    if(polylines.back().polyline_content.empty()) {
      polylines.back().polyline_content.push_back(graph[vd]);
    }
  }

  void add_edge(typename boost::graph_traits<Graph>::edge_descriptor ed)
  {
    typename boost::graph_traits<Graph>::vertex_descriptor
      s = source(ed, graph),
      t = target(ed, graph);
    Polyline_with_context& polyline = polylines.back();
    CGAL_assertion(!polyline.polyline_content.empty());
    if(polyline.polyline_content.back() != graph[s]) {
      polyline.polyline_content.push_back(graph[s]);
    } else if(polyline.polyline_content.back() != graph[t]) {
      // if the edge is zero-length, it is ignored
      polyline.polyline_content.push_back(graph[t]);
    }
    const typename boost::edge_bundle_type<Graph>::type &
      set_of_indices = graph[ed];
    polyline.context.adjacent_patches_ids.insert(set_of_indices.begin(),
                                                 set_of_indices.end());
  }

  void end_polyline()
  {
    // ignore degenerated polylines
    if(polylines.back().polyline_content.size() < 2)
      polylines.resize(polylines.size() - 1);
    // else {
    //   std::cerr << "Polyline with " << polylines.back().polyline_content.size()
    //             << " vertices, incident to "
    //             << polylines.back().context.adjacent_patches_ids.size()
    //             << " patches:\n ";
    //   for(auto p: polylines.back().polyline_content)
    //     std::cerr << " " << p;
    //   std::cerr << "\n";
    // }
  }
};


} // end CGAL::internal::Mesh_3
} // end CGAL::internal

/**
 * @class Polyhedral_complex_mesh_domain_3
 *
 *
 */
template < class IGT_,
           class Polyhedron_ = typename Mesh_polyhedron_3<IGT_>::type,
           class TriangleAccessor=Triangle_accessor_3<Polyhedron_,IGT_>,
           class Use_patch_id_tag = Tag_true,
           class Use_exact_intersection_construction_tag = Tag_true >
class Polyhedral_complex_mesh_domain_3
  : public Mesh_domain_with_polyline_features_3<
      Polyhedral_mesh_domain_3< Polyhedron_,
                                IGT_,
                                TriangleAccessor,
                                Use_patch_id_tag,
                                Use_exact_intersection_construction_tag > >
{
  typedef Mesh_domain_with_polyline_features_3<
    Polyhedral_mesh_domain_3<
      Polyhedron_, IGT_, TriangleAccessor,
      Use_patch_id_tag, Use_exact_intersection_construction_tag > > Base;

  typedef boost::adjacency_list<
    boost::setS, // this avoids parallel edges
    boost::vecS,
    boost::undirectedS,
    typename Polyhedron_::Point,
    typename Polyhedron_::Vertex::Set_of_indices> Featured_edges_copy_graph;

public:
  typedef Polyhedron_ Polyhedron;

  typedef IGT_                                IGT;
  typedef typename Base::Ray_3                Ray_3;
  typedef typename Base::Index                Index;
  // Index types
  typedef typename Base::Corner_index         Corner_index;
  typedef typename Base::Curve_segment_index  Curve_segment_index;
  typedef typename Base::Surface_patch_index  Surface_patch_index;
  typedef Surface_patch_index Patch_id;
  typedef typename Base::Subdomain_index      Subdomain_index;

  typedef typename Base::Subdomain            Subdomain;
  typedef typename Base::Bounding_box         Bounding_box;
  typedef typename Base::AABB_tree            AABB_tree;
  typedef typename Base::AABB_primitive       AABB_primitive;
  typedef typename Base::AABB_primitive_id    AABB_primitive_id;
  // Backward compatibility
#ifndef CGAL_MESH_3_NO_DEPRECATED_SURFACE_INDEX
  typedef Surface_patch_index                 Surface_index;
#endif // CGAL_MESH_3_NO_DEPRECATED_SURFACE_INDEX

  typedef typename Base::R         R;
  typedef typename Base::Point_3   Point_3;
  typedef typename Base::FT        FT;
  
  typedef CGAL::Tag_true           Has_features;

  typedef std::vector<Point_3> Bare_polyline;
  typedef Mesh_3::Polyline_with_context<Surface_patch_index, Curve_segment_index,
                                        Bare_polyline > Polyline_with_context;
  /// Constructors
  template <typename InputPolyhedraIterator,
            typename InputPairOfSubdomainIndicesIterator>
  Polyhedral_complex_mesh_domain_3
  ( InputPolyhedraIterator begin,
    InputPolyhedraIterator end,
    InputPairOfSubdomainIndicesIterator indices_begin,
    InputPairOfSubdomainIndicesIterator indices_end,
    CGAL::Random* p_rng = NULL )
    : Base(p_rng)
    , patch_indices(indices_begin, indices_end)
    , borders_detected_(false)
  {
    stored_polyhedra.reserve(std::distance(begin, end));
    CGAL_assertion(stored_polyhedra.capacity() ==
                   std::size_t(std::distance(indices_begin, indices_end)));
    for (; begin != end; ++begin) {
      stored_polyhedra.push_back(*begin);
      this->add_primitives(stored_polyhedra.back());
    }
    this->build();
  }

  /// Destructor
  ~Polyhedral_complex_mesh_domain_3() {}

  /// Detect features
  void initialize_ts(Polyhedron& p);

  void detect_features(std::vector<Polyhedron>& p);
  void detect_features() { detect_features(stored_polyhedra); }

  const std::vector<Polyhedron>& polyhedra() const {
    return stored_polyhedra;
  }

  const std::pair<Subdomain_index, Subdomain_index>&
  incident_subdomains_indices(Surface_patch_index patch_id) const
  {
    CGAL_assertion(patch_id > 0 &&
                   std::size_t(patch_id) < patch_id_to_polyhedron_id.size());

    const std::size_t polyhedron_id = this->patch_id_to_polyhedron_id[patch_id];
    return this->patch_indices[polyhedron_id];
  }

  const std::vector<Surface_patch_index>& boundary_patches() const
  {
    return this->boundary_patches_ids;
  }

  const std::vector<std::size_t>& inside_polyhedra() const
  {
    return this->inside_polyhedra_ids;
  }

  const std::vector<std::size_t>& boundary_polyhedra() const
  {
    return this->boundary_polyhedra_ids;
  }

  void compute_boundary_patches()
  {
    if (!this->boundary_patches_ids.empty())
      return;
    //patches are numbered from 1 to n
    //patch_id_to_polyhedron_id has size n+1
    for (Surface_patch_index patch_id = 1;
      static_cast<std::size_t>(patch_id) < patch_id_to_polyhedron_id.size();
      ++patch_id)
    {
      const std::pair<Subdomain_index, Subdomain_index>& subdomains
        = incident_subdomains_indices(patch_id);
      if (subdomains.first == 0 || subdomains.second == 0)
        this->boundary_patches_ids.push_back(patch_id);
    }
    for(std::size_t poly_id = 0, end = stored_polyhedra.size();
        poly_id < end; ++poly_id)
    {
      const std::pair<Subdomain_index, Subdomain_index>& subdomains
        = this->patch_indices[poly_id];
      if (subdomains.first != 0 && subdomains.second != 0)
        this->inside_polyhedra_ids.push_back(poly_id);
      else
        this->boundary_polyhedra_ids.push_back(poly_id);
    }
  }

  template <typename C3t3>
  void add_vertices_to_c3t3_on_patch_without_feature_edges(C3t3& c3t3) const {
    typedef typename C3t3::Triangulation Tr;
    Tr& tr = c3t3.triangulation();
    const std::size_t nb_of_patch_plus_one =
      this->several_vertices_on_patch.size();
    for(Patch_id patch_id = 1; std::size_t(patch_id) < nb_of_patch_plus_one;
        ++patch_id)
    {
      if(this->patch_has_featured_edges.test(patch_id)) continue;
      BOOST_FOREACH(Vertex_handle v, this->several_vertices_on_patch[patch_id])
      {
        typename Tr::Vertex_handle tv = tr.insert(v->point());
        c3t3.set_dimension(tv, 2);
        c3t3.set_index(tv, patch_id);
      }
    }
  }

  bool is_sharp(const Polyhedron& p,
                const typename Polyhedron::Halfedge_handle& he,
                FT cos_angle) const
  {
    typename Polyhedron::Facet_handle f1 = he->facet();
    typename Polyhedron::Facet_handle f2 = he->opposite()->facet();
    if(f1 == NULL || f2 == NULL)
      return true;
    const typename IGT::Point_3& p1 = he->vertex()->point();
    const typename IGT::Point_3& p2 = he->opposite()->vertex()->point();
    if( (p1.x() == p2.x()) +
        (p1.y() == p2.y()) +
        (p1.z() == p2.z()) < 2 ) return false;
    using CGAL::Polygon_mesh_processing::compute_face_normal;
    const typename IGT::Vector_3& n1 = compute_face_normal(f1, p);
    const typename IGT::Vector_3& n2 = compute_face_normal(f2, p);

    if ( n1 * n2 <= cos_angle )
      return true;
    else
      return false;
  }

  void detect_sharp_edges(Polyhedron& polyhedron, FT angle_in_deg) const
  {
    // Initialize vertices
    for(typename Polyhedron::Vertex_iterator v = polyhedron.vertices_begin(),
          end = polyhedron.vertices_end() ; v != end ; ++v)
    {
      v->nb_of_feature_edges = 0;
    }

    const FT cos_angle(std::cos(CGAL::to_double(angle_in_deg)*CGAL_PI /180.));

    // Detect sharp edges
    for(typename Polyhedron::Halfedge_iterator he = polyhedron.edges_begin(),
          end = polyhedron.edges_end() ; he != end ; ++he)
    {
      if(this->is_sharp(polyhedron, he, cos_angle))
      {
        he->set_feature_edge(true);
        he->opposite()->set_feature_edge(true);

        ++he->vertex()->nb_of_feature_edges;
        ++he->opposite()->vertex()->nb_of_feature_edges;
      }
    }
  }

  static void detect_border_edges(Polyhedron& polyhedron)
  {
    // Initialize vertices
    for(typename Polyhedron::Vertex_iterator v = polyhedron.vertices_begin(),
          end = polyhedron.vertices_end() ; v != end ; ++v)
    {
      v->nb_of_feature_edges = 0;
    }

    // Detect border edges
    for(typename Polyhedron::Halfedge_iterator he = polyhedron.edges_begin(),
          end = polyhedron.edges_end() ; he != end ; ++he)
    {
      if(he->is_border())
      {
        he->set_feature_edge(true);
        he->opposite()->set_feature_edge(true);

        ++he->vertex()->nb_of_feature_edges;
        ++he->opposite()->vertex()->nb_of_feature_edges;
      }
    }
  }

  struct Is_in_domain
  {
    Is_in_domain(const Polyhedral_complex_mesh_domain_3& domain)
      : r_domain_(domain) {}

    boost::optional<AABB_primitive_id> shoot_a_ray_1(const Ray_3 ray) const {
      return r_domain_.bounding_aabb_tree_ptr()->
        first_intersected_primitive(ray);
    }

#if USE_ALL_INTERSECTIONS
    boost::optional<AABB_primitive_id> shoot_a_ray_2(const Ray_3 ray) const {
      const Point_3& p = ray.source();
      typedef typename AABB_tree::
        template Intersection_and_primitive_id<Ray_3>::Type Inter_and_prim;
      std::vector<Inter_and_prim> all_intersections;
      r_domain_.bounding_aabb_tree_ptr()->
        all_intersections(ray, std::back_inserter(all_intersections));
      if(all_intersections.empty())
        return boost::none;
      else {
        for(const Inter_and_prim& i_p: all_intersections) {
          if(boost::get<Point_3>( &i_p.first) == 0) return AABB_primitive_id();
        }
        auto it = std::min_element
          (all_intersections.begin(), all_intersections.end(),
           [p](const Inter_and_prim& a,
               const Inter_and_prim& b)
           {
             const Point_3& pa = boost::get<Point_3>(a.first);
             const Point_3& pb = boost::get<Point_3>(b.first);
             return compare_distance_to_point(p, pa, pb)
             == CGAL::SMALLER;
           });
        return it->second;
      }
    }
#endif // USE_ALL_INTERSECTIONS

    Subdomain operator()(const Point_3& p) const {
      if(r_domain_.bounding_aabb_tree_ptr() == 0) return Subdomain();
      const Bounding_box bbox = r_domain_.bounding_aabb_tree_ptr()->bbox();

      if(   p.x() < bbox.xmin() || p.x() > bbox.xmax()
            || p.y() < bbox.ymin() || p.y() > bbox.ymax()
            || p.z() < bbox.zmin() || p.z() > bbox.zmax() )
      {
        return Subdomain();
      }
  
      // Shoot ray
      typename IGT::Construct_ray_3 ray = IGT().construct_ray_3_object();
      typename IGT::Construct_vector_3 vector = IGT().construct_vector_3_object();

      while(true) {
        Random_points_on_sphere_3<Point_3> random_point(1.);

        const Ray_3 ray_shot = ray(p, vector(CGAL::ORIGIN,*random_point));

#define USE_ALL_INTERSECTIONS 0
#if USE_ALL_INTERSECTIONS
        boost::optional<AABB_primitive_id> opt = shoot_a_ray_2(ray_shot);
#else // first_intersected_primitive
        boost::optional<AABB_primitive_id> opt = shoot_a_ray_1(ray_shot);
#endif // first_intersected_primitive
        // for(int i = 0; i < 20; ++i) {
        //   const Ray_3 ray_shot2 = ray(p, vector(CGAL::ORIGIN,*random_point));
        //   boost::optional<AABB_primitive_id> opt2 = shoot_a_ray_1(ray_shot2);
        //   if(opt != opt2) {
        //     if(!opt  && *opt2 == 0) continue;
        //     if(!opt2 && *opt  == 0) continue;
        //     std::cerr << "Not the same result for:\n  "
        //               << ray_shot
        //               << "\n  " << ray_shot2 << std::endl;
        //     abort();
        //   }
        // }
        if(!opt)
          return Subdomain();
        else {
          typename Polyhedron::Facet_const_handle fh = *opt;
          if(fh == AABB_primitive_id()) continue; // loop
          const std::pair<Subdomain_index, Subdomain_index>& pair =
            r_domain_.incident_subdomains_indices(fh->patch_id());
          typename Polyhedron::Halfedge_const_handle he = fh->halfedge();
          const Point_3& a = he->vertex()->point();
          const Point_3& b = he->next()->vertex()->point();
          const Point_3& c = he->next()->next()->vertex()->point();
          switch(orientation(a, b, c, p)) {
          case NEGATIVE: // inner region
            return pair.first == 0 ?
              Subdomain() :
              Subdomain(Subdomain_index(pair.first));
          case POSITIVE: // outer region
            return pair.second == 0 ?
              Subdomain() :
              Subdomain(Subdomain_index(pair.second));
          default: /* COPLANAR */ continue; // loop
          } // end switch on the orientation
        } // opt
      } // end while(true)
    } // end operator()
  private:
    const Polyhedral_complex_mesh_domain_3& r_domain_;
  };
  Is_in_domain is_in_domain_object() const { return Is_in_domain(*this); }

private:
  void add_features_from_split_graph_into_polylines(Featured_edges_copy_graph& graph);

  template <typename Edge_predicate, typename P2Vmap>
  void add_featured_edges_to_graph(const Polyhedron& poly,
                                   const Edge_predicate& pred,
                                   Featured_edges_copy_graph& graph,
                                   P2Vmap& p2vmap);

  std::vector<Polyhedron> stored_polyhedra;
  std::vector<std::pair<Subdomain_index, Subdomain_index> > patch_indices;
  std::vector<std::size_t> patch_id_to_polyhedron_id;
  boost::dynamic_bitset<> patch_has_featured_edges;
  typedef typename Polyhedron::Vertex_handle Vertex_handle;
  std::vector<std::vector<Vertex_handle> > several_vertices_on_patch;
  std::vector<Surface_patch_index> boundary_patches_ids;
  std::vector<std::size_t> inside_polyhedra_ids;
  std::vector<std::size_t> boundary_polyhedra_ids;
  bool borders_detected_;

private:
  // Disabled copy constructor & assignment operator
  typedef Polyhedral_complex_mesh_domain_3 Self;
  Polyhedral_complex_mesh_domain_3(const Self& src);
  Self& operator=(const Self& src);

};  // end class Polyhedral_complex_mesh_domain_3


template < typename GT_, typename P_, typename TA_,
           typename Tag_, typename E_tag_>
void
Polyhedral_complex_mesh_domain_3<GT_,P_,TA_,Tag_,E_tag_>::
initialize_ts(Polyhedron& p)
{
  std::size_t ts = 0;
  for(typename Polyhedron::Vertex_iterator v = p.vertices_begin(),
      end = p.vertices_end() ; v != end ; ++v)
  {
    v->set_time_stamp(ts++);
  }
  for(typename Polyhedron::Facet_iterator fit = p.facets_begin(),
       end = p.facets_end() ; fit != end ; ++fit )
  {
    fit->set_time_stamp(ts++);
  }
  for(typename Polyhedron::Halfedge_iterator hit = p.halfedges_begin(),
       end = p.halfedges_end() ; hit != end ; ++hit )
  {
    hit->set_time_stamp(ts++);
  }
}




template <typename Graph>
void dump_graph_edges(std::ostream& out, const Graph& g)
{
  typedef typename boost::graph_traits<Graph>::vertex_descriptor vertex_descriptor;
  typedef typename boost::graph_traits<Graph>::edge_descriptor edge_descriptor;

  out.precision(17);
  BOOST_FOREACH(edge_descriptor e, edges(g))
  {
    vertex_descriptor s = source(e, g);
    vertex_descriptor t = target(e, g);
    out << "2 " << g[s] << " " << g[t] << "\n";
  }
}

template <typename Graph>
void dump_graph_edges(const char* filename, const Graph& g)
{
  std::ofstream out(filename);
  dump_graph_edges(out, g);
}

template < typename GT_, typename P_, typename TA_,
           typename Tag_, typename E_tag_>
void
Polyhedral_complex_mesh_domain_3<GT_,P_,TA_,Tag_,E_tag_>::
detect_features(std::vector<Polyhedron>& poly)
{
  CGAL_assertion(!borders_detected_);
  if (borders_detected_)
    return;//prevent from not-terminating

  typedef Featured_edges_copy_graph G_copy;
  G_copy g_copy;
  typedef typename boost::graph_traits<G_copy>::vertex_descriptor vertex_descriptor;
  typedef std::map<typename Polyhedron::Point,
                   vertex_descriptor> P2vmap;
  // TODO: replace this map by and unordered_map
  P2vmap p2vmap;

  CGAL::Mesh_3::Detect_features_in_polyhedra<Polyhedron> detect_features;
  BOOST_FOREACH(Polyhedron& p, poly)
  {
    initialize_ts(p);

    double angle = 180;
    std::size_t poly_id = &p-&poly[0];
#if CGAL_MESH_3_VERBOSE
    std::cerr << "Polyhedron #" << poly_id << " :\n";
    std::cerr << "  material #" << patch_indices[poly_id].first << "\n";
    std::cerr << "  material #" << patch_indices[poly_id].second << "\n";
#endif // CGAL_MESH_3_VERBOSE
    if(patch_indices[poly_id].first == 0 ||
       patch_indices[poly_id].second == 0)
    {
      angle = 60;
    }
#if CGAL_MESH_3_VERBOSE
    std::cerr << "  angle: " << angle << "\n";
#endif
    // Get sharp features
    if(angle == 180)
      this->detect_border_edges(p);
    else
      this->detect_sharp_edges(p, angle);
    detect_features.detect_surface_patches(p);
    detect_features.detect_vertices_incident_patches(p);

    internal::Mesh_3::Is_featured_edge<Polyhedron> is_featured_edge(p);

    add_featured_edges_to_graph(p, is_featured_edge, g_copy, p2vmap);
  }
  const std::size_t nb_of_patch_plus_one =
    detect_features.maximal_surface_patch_index()+1;
  this->patch_id_to_polyhedron_id.resize(nb_of_patch_plus_one);
  this->patch_has_featured_edges.resize(nb_of_patch_plus_one);
  this->several_vertices_on_patch.resize(nb_of_patch_plus_one);
#if CGAL_MESH_3_VERBOSE
  std::cerr << "Number of patches: " << (nb_of_patch_plus_one - 1) << std::endl;
#endif
  BOOST_FOREACH(Polyhedron& p, poly)
  {
    const std::size_t polyhedron_id = &p - &poly[0];
    BOOST_FOREACH(typename Polyhedron::Facet_const_handle fh, faces(p))
    {
      patch_id_to_polyhedron_id[fh->patch_id()] = polyhedron_id;
    }
    for(typename Polyhedron::Halfedge_iterator
          heit = p.halfedges_begin(), end  = p.halfedges_end();
        heit != end; ++heit)
    {
      if(is_border(heit, p) || !heit->is_feature_edge()) continue;
      patch_has_featured_edges.set(heit->face()->patch_id());
    }
    for(typename Polyhedron::Vertex_iterator
          vit = p.vertices_begin(), end  = p.vertices_end();
        vit != end; ++vit)
    {
      if( vit->is_feature_vertex() ) { continue; }
      const Patch_id patch_id = vit->halfedge()->face()->patch_id();
      if(patch_has_featured_edges.test(patch_id)) continue;
      several_vertices_on_patch[patch_id].push_back(vit);
    }
  }
  for(Patch_id patch_id = 1; std::size_t(patch_id) < nb_of_patch_plus_one;
      ++patch_id)
  {
    CGAL_assertion(patch_has_featured_edges.test(patch_id) ==
                   several_vertices_on_patch[patch_id].empty() );
    if(several_vertices_on_patch[patch_id].empty()) continue;
    std::random_shuffle(several_vertices_on_patch[patch_id].begin(),
                        several_vertices_on_patch[patch_id].end());
    if(several_vertices_on_patch[patch_id].size()>20)
      several_vertices_on_patch[patch_id].resize(20);
#if __cplusplus > 201103L
    several_vertices_on_patch.shrink_to_fit();
#endif
  }
  add_features_from_split_graph_into_polylines(g_copy);
  borders_detected_ = true;/*done by Mesh_3::detect_features*/

  this->compute_boundary_patches();
}

template < typename GT_, typename P_, typename TA_,
           typename Tag_, typename E_tag_>
void
Polyhedral_complex_mesh_domain_3<GT_,P_,TA_,Tag_,E_tag_>::
add_features_from_split_graph_into_polylines(Featured_edges_copy_graph& g_copy)
{
  std::vector<Polyline_with_context> polylines;

  internal::Mesh_3::Extract_polyline_with_context_visitor<
    Polyhedral_complex_mesh_domain_3,
    Polyline_with_context,
    Featured_edges_copy_graph
    > visitor(g_copy, polylines);
  internal::Mesh_3::Angle_tester<GT_> angle_tester;
  split_graph_into_polylines(g_copy, visitor, angle_tester);

  this->add_features_with_context(polylines.begin(),
                                  polylines.end());

#if CGAL_MESH_3_PROTECTION_DEBUG & 2
  {//DEBUG
    std::ofstream og("polylines_graph.polylines.txt");
    og.precision(17);
    BOOST_FOREACH(const Polyline_with_context& poly, polylines)
    {
      og << poly.polyline_content.size() << " ";
      BOOST_FOREACH(const Point_3& p, poly.polyline_content)
        og << p << " ";
      og << std::endl;
    }
    og.close();
  }
#endif // CGAL_MESH_3_PROTECTION_DEBUG & 2

}

template < typename GT_, typename P_, typename TA_,
           typename Tag_, typename E_tag_>
template <typename Edge_predicate, typename P2vmap>
void
Polyhedral_complex_mesh_domain_3<GT_,P_,TA_,Tag_,E_tag_>::
add_featured_edges_to_graph(const Polyhedron& p,
                            const Edge_predicate& pred,
                            Featured_edges_copy_graph& g_copy,
                            P2vmap& p2vmap)
{
  typedef boost::filtered_graph<Polyhedron,
                                Edge_predicate > Featured_edges_graph;
  Featured_edges_graph orig_graph(p, pred);

  typedef Featured_edges_graph Graph;
  typedef typename boost::graph_traits<Graph>::vertex_descriptor Graph_vertex_descriptor;
  typedef typename boost::graph_traits<Graph>::edge_descriptor Graph_edge_descriptor;
  typedef Featured_edges_copy_graph G_copy;
  typedef typename boost::graph_traits<G_copy>::vertex_descriptor vertex_descriptor;
  typedef typename boost::graph_traits<G_copy>::edge_descriptor edge_descriptor;

  const Featured_edges_graph& graph = orig_graph;

  BOOST_FOREACH(Graph_vertex_descriptor v, vertices(graph)){
    vertex_descriptor vc;
    typename P2vmap::iterator it = p2vmap.find(v->point());
    if(it == p2vmap.end()) {
      vc = add_vertex(g_copy);
      g_copy[vc] = v->point();
      p2vmap[v->point()] = vc;
    }
  }

  BOOST_FOREACH(Graph_edge_descriptor e, edges(graph)){
    vertex_descriptor vs = p2vmap[source(e,graph)->point()];
    vertex_descriptor vt = p2vmap[target(e,graph)->point()];
    CGAL_warning_msg(vs != vt, "ignore self loop");
    if(vs != vt) {
      const std::pair<edge_descriptor, bool> pair = add_edge(vs,vt,g_copy);
      typename Polyhedron::Halfedge_handle he = halfedge(e, p);
      if(!is_border(he, p)) {
        g_copy[pair.first].insert(he->face()->patch_id());;
      }
      he = he->opposite();
      if(!is_border(he, p)) {
        g_copy[pair.first].insert(he->face()->patch_id());;
      }
    }
  }

#if CGAL_MESH_3_PROTECTION_DEBUG > 1
  {// DEBUG
    dump_graph_edges("edges-graph.polylines.txt", g_copy);
  }
#endif
}


} //namespace CGAL


#endif // CGAL_POLYHEDRAL_COMPLEX_MESH_DOMAIN_3_H
