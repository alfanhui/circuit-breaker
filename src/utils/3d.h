/*
** Jo Sega Saturn Engine
** Copyright (c) 2012-2013, Johannes Fetz (johannesfetz@gmail.com)
** All rights reserved.
*/

/** @brief Plane Rectangle vertice table */
# define JO_3D_PLANE_RECTANGLE_VERTICES_Z(WIDTH, HEIGHT) \
{ \
  { -WIDTH , -HEIGHT , 0 }, \
  { WIDTH  , -HEIGHT , 0 }, \
  { WIDTH  , HEIGHT  , 0 }, \
  { -WIDTH , HEIGHT  , 0 } \
}

/** @brief Plane Rectangle vertice table */
# define JO_3D_PLANE_RECTANGLE_VERTICES_X(WIDTH, HEIGHT) \
{ \
  { -WIDTH , 0, -HEIGHT }, \
  { WIDTH  , 0, -HEIGHT }, \
  { WIDTH  , 0, HEIGHT  }, \
  { -WIDTH , 0, HEIGHT  } \
}

/** @brief Plane Rectangle vertice table */
# define JO_3D_PLANE_RECTANGLE_VERTICES_X_2(WIDTH, HEIGHT) \
{ \
  { -WIDTH      , 0,  -HEIGHT }, \
  { WIDTH / 2   , 0,  -HEIGHT }, \
  { WIDTH / 2   , 0,  HEIGHT  }, \
  { -WIDTH      , 0,  HEIGHT  }, \
  { -WIDTH /2   , 0,  -HEIGHT }, \
  { WIDTH       , 0,  -HEIGHT }, \
  { WIDTH       , 0,  HEIGHT  }, \
  { -WIDTH /2   , 0,  HEIGHT  } \
}

/** @brief Plane Rectangle vertice table */
# define JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(WIDTH, HEIGHT) \
{ \
  { -WIDTH , 0, -HEIGHT }, \
  { WIDTH  , 0, -HEIGHT }, \
  { WIDTH  , 0, HEIGHT  }, \
  { -WIDTH , 0, HEIGHT  } \
}

/** @brief Plane Rectangle vertice table */
# define JO_3D_PLANE_RECTANGLE_VERTICES_Y(WIDTH, HEIGHT) \
{ \
  { 0, -WIDTH , -HEIGHT }, \
  { 0, WIDTH  , -HEIGHT }, \
  { 0, WIDTH  , HEIGHT  }, \
  { 0, -WIDTH , HEIGHT  } \
}

/** @brief Plane Rectangle Flipped vertice table */
# define JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(WIDTH, HEIGHT) \
{ \
  { 0, -WIDTH , HEIGHT  }, \
  { 0, WIDTH  , HEIGHT  }, \
  { 0, WIDTH  , -HEIGHT }, \
  { 0, -WIDTH , -HEIGHT } \
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
/*
** END OF FILE
*/
