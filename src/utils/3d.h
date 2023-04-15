/*
** Jo Sega Saturn Engine
** Copyright (c) 2012-2013, Johannes Fetz (johannesfetz@gmail.com)
** All rights reserved.
*/

/** @brief Plane Rectangle vertice table */
# define JO_3D_PLANE_RECTANGLE_VERTICES_Z(WIDTH, HEIGHT) \
{ \
  { WIDTH , HEIGHT , 0 }, \
  { -WIDTH  , HEIGHT , 0 }, \
  { -WIDTH  , -HEIGHT  , 0 }, \
  { WIDTH , -HEIGHT  , 0 } \
}

/** @brief Plane Rectangle vertice table */
# define JO_3D_PLANE_RECTANGLE_VERTICES_X(WIDTH, HEIGHT) \
{ \
  { WIDTH  , 0, HEIGHT   }, \
  { -WIDTH , 0, HEIGHT   }, \
  { -WIDTH , 0, -HEIGHT  }, \
  { WIDTH  , 0, -HEIGHT  } \
}

/** @brief Plane Rectangle vertice table */
# define JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(WIDTH, HEIGHT) \
{ \
    { -WIDTH , 0, HEIGHT   }, \
    { WIDTH , 0, HEIGHT  }, \
    { WIDTH  , 0, -HEIGHT  }, \
    { -WIDTH  , 0, -HEIGHT   } \
}

/** @brief Plane Rectangle vertice table */
# define JO_3D_PLANE_RECTANGLE_VERTICES_Y(WIDTH, HEIGHT) \
{ \
  { 0, WIDTH , HEIGHT }, \
  { 0, -WIDTH  , HEIGHT }, \
  { 0, -WIDTH  , -HEIGHT  }, \
  { 0, WIDTH , -HEIGHT  } \
}

/** @brief Plane Rectangle Flipped vertice table */
# define JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(WIDTH, HEIGHT) \
{ \
  { 0, WIDTH , -HEIGHT  }, \
  { 0, -WIDTH  , -HEIGHT  }, \
  { 0, -WIDTH  , HEIGHT }, \
  { 0, WIDTH , HEIGHT } \
}

# define JO_3D_CUBE_CUSTOM_VERTICES(WIDTH, HEIGHT, DEPTH) \
{ \
  { -WIDTH, -HEIGHT, DEPTH }, \
  { WIDTH, -HEIGHT, DEPTH }, \
  { WIDTH, HEIGHT, DEPTH }, \
  { -WIDTH, HEIGHT, DEPTH }, \
  \
  { -WIDTH, -HEIGHT, -DEPTH }, \
  { -WIDTH, -HEIGHT, DEPTH }, \
  { -WIDTH, HEIGHT, DEPTH }, \
  { -WIDTH, HEIGHT, -DEPTH }, \
  \
  { WIDTH, -HEIGHT, -DEPTH }, \
  { -WIDTH, -HEIGHT, -DEPTH }, \
  { -WIDTH, HEIGHT, -DEPTH }, \
  { WIDTH, HEIGHT, -DEPTH }, \
  \
  { WIDTH, -HEIGHT, DEPTH }, \
  { WIDTH, -HEIGHT, -DEPTH }, \
  { WIDTH, HEIGHT, -DEPTH }, \
  { WIDTH, HEIGHT, DEPTH }, \
  \
  { -WIDTH, -HEIGHT, -DEPTH }, \
  { WIDTH, -HEIGHT, -DEPTH }, \
  { WIDTH, -HEIGHT, DEPTH }, \
  { -WIDTH, -HEIGHT, DEPTH }, \
  \
  { -WIDTH, HEIGHT, DEPTH }, \
  { WIDTH, HEIGHT, DEPTH }, \
  { WIDTH, HEIGHT, -DEPTH }, \
  { -WIDTH, HEIGHT, -DEPTH } \
}



/** @brief Plane Rectangle vertice table */
# define JO_3D_TRAIL_PLANE_VERTICES_X(X1, X2, HEIGHT) \
{ \
  {   X1 , 0, HEIGHT  }, \
  {   X1 , 0, 0   }, \
  {   X2 , 0, 0   }, \
  {   X2 , 0, HEIGHT  }  \
}

/** @brief Plane Rectangle vertice table */
# define JO_3D_TRAIL_PLANE_VERTICES_X_NEW(X1, X2, Y1, Y2, HEIGHT) \
{ \
  {   X1 , Y1, HEIGHT  }, \
  {   X1 , Y1, 0   }, \
  {   X2 , Y2, 0   }, \
  {   X2 , Y2, HEIGHT  }  \
}

/** @brief Plane Rectangle vertice table */
# define JO_3D_TRAIL_PLANE_VERTICES_Y(Y1, Y2, HEIGHT) \
{ \
  {  0, Y1,  0 }, \
  {  0, Y1,  HEIGHT  }, \
  {  0, Y2,  HEIGHT  }, \
  {  0, Y2,  0 }  \
}

/** @brief Plane Rectangle vertice table */
# define JO_3D_TRAIL_PLANE_VERTICES_Y_NEW(X1, X2, Y1, Y2, HEIGHT) \
{ \
  {  X1, Y1,  0 }, \
  {  X1, Y1,  HEIGHT  }, \
  {  X2, Y2,  HEIGHT  }, \
  {  X2, Y2,  0 }  \
}
/*
** END OF FILE
*/