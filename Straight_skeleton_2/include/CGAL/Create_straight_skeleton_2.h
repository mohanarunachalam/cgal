// Copyright (c) 2006 Fernando Luis Cacciola Carballal. All rights reserved.
//
// This file is part of CGAL (www.cgal.org); you may redistribute it under
// the terms of the Q Public License version 1.0.
// See the file LICENSE.QPL distributed with CGAL.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//

// $URL: svn+ssh://CGAL/svn/cgal/trunk/Straight_skeleton_2/include/CGAL/Straight_skeleton_builder_2.h $
// $Id: Straight_skeleton_builder_2.h 40951 2007-11-19 16:33:25Z fcacciola $
//
// Author(s)     : Fernando Cacciola <fernando_cacciola@ciudad.com.ar>
//
#ifndef CGAL_CREATE_STRAIGHT_SKELETON_2_H
#define CGAL_CREATE_STRAIGHT_SKELETON_2_H

#include <CGAL/Straight_skeleton_builder_2.h>
#include <CGAL/compute_outer_frame_margin.h>
#include <CGAL/Polygon_2.h>

CGAL_BEGIN_NAMESPACE


template<class Poly>
inline typename Poly::const_iterator vertices_begin ( Poly const& aPoly ) { return aPoly.begin() ; }

template<class Poly>
inline typename Poly::const_iterator vertices_end ( Poly const& aPoly ) { return aPoly.end() ; }


template<class K>
inline typename Polygon_2<K>::Vertex_const_iterator vertices_begin ( Polygon_2<K> const& aPoly ) 
{ return aPoly.vertices_begin() ; }

template<class K>
inline typename Polygon_2<K>::Vertex_const_iterator vertices_end( Polygon_2<K> const& aPoly ) 
{ return aPoly.vertices_end() ; }

template<class Poly>
inline Poly reverse_polygon ( Poly const& aPoly ) { return Poly( aPoly.rbegin(), aPoly.rend() ) ; }

template<class K>
inline Polygon_2<K> reverse_polygon( Polygon_2<K> const& aPoly ) 
{
  Polygon_2<K> r ( aPoly ) ;
  r.reverse_orientation();
  return r ;  
}

template<class PointIterator, class HoleIterator, class K>
boost::shared_ptr< Straight_skeleton_2<K> >
create_interior_straight_skeleton_2 ( PointIterator aOuterContour_VerticesBegin
                                    , PointIterator aOuterContour_VerticesEnd
                                    , HoleIterator  aHolesBegin
                                    , HoleIterator  aHolesEnd
                                    , K const&      
                                    )
{
  typedef Straight_skeleton_2<K> Ss ;
  typedef boost::shared_ptr<Ss>  SsPtr ;

  typedef Straight_skeleton_builder_traits_2<K> SsBuilderTraits;
  
  typedef Straight_skeleton_builder_2<SsBuilderTraits,Ss> SsBuilder;
  
  typedef typename std::iterator_traits<PointIterator>::value_type InputPoint ;
  typedef typename Kernel_traits<InputPoint>::Kernel InputKernel ;
  
  Cartesian_converter<InputKernel, K> Point_converter ;
  
  SsBuilder ssb ;
  
  ssb.enter_contour( aOuterContour_VerticesBegin, aOuterContour_VerticesEnd, Point_converter ) ;
  
  for ( HoleIterator hi = aHolesBegin ; hi != aHolesEnd ; ++ hi )
    ssb.enter_contour( vertices_begin(*hi), vertices_end(*hi), Point_converter ) ;
  
  return ssb.construct_skeleton();
}

template<class PointIterator, class HoleIterator>
boost::shared_ptr< Straight_skeleton_2< Exact_predicates_inexact_constructions_kernel > >
inline 
create_interior_straight_skeleton_2 ( PointIterator aOuterContour_VerticesBegin
                                    , PointIterator aOuterContour_VerticesEnd
                                    , HoleIterator  aHolesBegin
                                    , HoleIterator  aHolesEnd
                                    )
{
  return create_interior_straight_skeleton_2(aOuterContour_VerticesBegin
                                            ,aOuterContour_VerticesEnd
                                            ,aHolesBegin
                                            ,aHolesEnd
                                            ,Exact_predicates_inexact_constructions_kernel()
                                            );
}

template<class PointIterator, class K>
boost::shared_ptr< Straight_skeleton_2<K> >
inline
create_interior_straight_skeleton_2 ( PointIterator aOuterContour_VerticesBegin
                                    , PointIterator aOuterContour_VerticesEnd 
                                    , K const&      k
                                    )
{
  std::vector< Polygon_2<K> > no_holes ;
  return create_interior_straight_skeleton_2(aOuterContour_VerticesBegin
                                            ,aOuterContour_VerticesEnd
                                            ,no_holes.begin()
                                            ,no_holes.end()
                                            ,k 
                                            );
}

template<class PointIterator>
boost::shared_ptr< Straight_skeleton_2<Exact_predicates_inexact_constructions_kernel> >
inline
create_interior_straight_skeleton_2 ( PointIterator aOuterContour_VerticesBegin
                                    , PointIterator aOuterContour_VerticesEnd 
                                    )
{
  return create_interior_straight_skeleton_2(aOuterContour_VerticesBegin
                                            ,aOuterContour_VerticesEnd
                                            ,Exact_predicates_inexact_constructions_kernel() 
                                            );
}

template<class Polygon, class K>
boost::shared_ptr< Straight_skeleton_2<K> >
inline
create_interior_straight_skeleton_2 ( Polygon const& aOutContour, K const& k )
{
  return create_interior_straight_skeleton_2(vertices_begin(aOutContour), vertices_end(aOutContour), k );
}

template<class Polygon>
boost::shared_ptr< Straight_skeleton_2< Exact_predicates_inexact_constructions_kernel > >
inline
create_interior_straight_skeleton_2 ( Polygon const& aOutContour )
{
  return create_interior_straight_skeleton_2(aOutContour, Exact_predicates_inexact_constructions_kernel() );
}

template<class FT, class PointIterator, class HoleIterator, class K>
boost::shared_ptr< Straight_skeleton_2<K> >
create_exterior_straight_skeleton_2 ( FT             aMaxOffset
                                    , PointIterator  aOuterContour_VerticesBegin
                                    , PointIterator  aOuterContour_VerticesEnd
                                    , HoleIterator   aHolesBegin
                                    , HoleIterator   aHolesEnd
                                    , K const&       k
                                    , bool           aDontReverseOrientation = false
                                    )
{
  typedef typename std::iterator_traits<PointIterator>::value_type Point_2 ;
    
  typedef Straight_skeleton_2<K> Ss ;
  typedef boost::shared_ptr<Ss>  SsPtr ;
  
  SsPtr rSkeleton ;
  
  boost::optional<FT> margin = compute_outer_frame_margin( aOuterContour_VerticesBegin, aOuterContour_VerticesEnd, aMaxOffset );

  if ( margin )
  {
    
    Bbox_2 bbox = bbox_2( aOuterContour_VerticesBegin, aOuterContour_VerticesEnd);

    FT fxmin = bbox.xmin() - *margin ;
    FT fxmax = bbox.xmax() + *margin ;
    FT fymin = bbox.ymin() - *margin ;
    FT fymax = bbox.ymax() + *margin ;

    Point_2 frame[4] ;
    
    frame[0] = Point_2(fxmin,fymin) ;
    frame[1] = Point_2(fxmax,fymin) ;
    frame[2] = Point_2(fxmax,fymax) ;
    frame[3] = Point_2(fxmin,fymax) ;

    typedef typename HoleIterator::value_type Hole ;
    
    std::vector<Hole> holes ;
    
    Hole lOuterContour(aOuterContour_VerticesBegin, aOuterContour_VerticesEnd);
    
    holes.push_back( aDontReverseOrientation ? lOuterContour : reverse_polygon(lOuterContour) ) ;
        
    for ( HoleIterator hi = aHolesBegin ; hi != aHolesEnd ; ++ hi )
      holes.push_back( aDontReverseOrientation ? *hi : reverse_polygon(*hi) ) ;
      
    rSkeleton = create_interior_straight_skeleton_2(frame, frame+4, holes.begin(), holes.end(), k ) ;  
  }
  
  return rSkeleton ;
}

template<class FT, class PointIterator, class HoleIterator>
boost::shared_ptr< Straight_skeleton_2<Exact_predicates_inexact_constructions_kernel> >
inline
create_exterior_straight_skeleton_2 ( FT             aMaxOffset
                                    , PointIterator  aOuterContour_VerticesBegin
                                    , PointIterator  aOuterContour_VerticesEnd
                                    , HoleIterator   aHolesBegin
                                    , HoleIterator   aHolesEnd
                                    , bool           aDontReverseOrientation = false
                                    )
{
  return create_exterior_straight_skeleton_2(aMaxOffset
                                            ,aOuterContour_VerticesBegin
                                            ,aOuterContour_VerticesEnd
                                            ,aHolesBegin
                                            ,aHolesEnd
                                            ,Exact_predicates_inexact_constructions_kernel()
                                            ,aDontReverseOrientation
                                            );
}

template<class FT, class PointIterator, class K>
boost::shared_ptr< Straight_skeleton_2<K> >
inline
create_exterior_straight_skeleton_2 ( FT            aMaxOffset
                                    , PointIterator aOuterContour_VerticesBegin
                                    , PointIterator aOuterContour_VerticesEnd
                                    , K const&      k
                                    , bool          aDontReverseOrientation = false
                                    )
{
  std::vector< Polygon_2<K> > no_holes ;
  return create_exterior_straight_skeleton_2(aMaxOffset
                                            ,aOuterContour_VerticesBegin
                                            ,aOuterContour_VerticesEnd
                                            ,no_holes.begin()
                                            ,no_holes.end() 
                                            ,k
                                            ,aDontReverseOrientation
                                            );
}

template<class FT, class PointIterator>
boost::shared_ptr< Straight_skeleton_2<Exact_predicates_inexact_constructions_kernel> >
inline
create_exterior_straight_skeleton_2 ( FT            aMaxOffset
                                    , PointIterator aOuterContour_VerticesBegin
                                    , PointIterator aOuterContour_VerticesEnd
                                    , bool          aDontReverseOrientation = false
                                    )
{
  return create_exterior_straight_skeleton_2(aMaxOffset
                                            ,aOuterContour_VerticesBegin
                                            ,aOuterContour_VerticesEnd
                                            ,Exact_predicates_inexact_constructions_kernel()
                                            ,aDontReverseOrientation
                                            );
}

template<class FT, class Polygon, class K>
boost::shared_ptr< Straight_skeleton_2<K> >
inline
create_exterior_straight_skeleton_2 ( FT             aMaxOffset
                                    , Polygon const& aOutContour
                                    , K const&       k
                                    , bool           aDontReverseOrientation = false
                                    )
{
  return create_exterior_straight_skeleton_2(aMaxOffset
                                            ,vertices_begin(aOutContour)
                                            ,vertices_end  (aOutContour)
                                            ,k
                                            ,aDontReverseOrientation
                                            );
}

template<class FT, class Polygon>
boost::shared_ptr< Straight_skeleton_2<Exact_predicates_inexact_constructions_kernel> >
inline
create_exterior_straight_skeleton_2 ( FT             aMaxOffset
                                    , Polygon const& aOutContour
                                    , bool           aDontReverseOrientation = false
                                    )
{
  return create_exterior_straight_skeleton_2(aMaxOffset
                                            ,aOutContour
                                            ,Exact_predicates_inexact_constructions_kernel()
                                            ,aDontReverseOrientation
                                            );
}

CGAL_END_NAMESPACE


#endif // CGAL_STRAIGHT_SKELETON_BUILDER_2_H //
// EOF //
