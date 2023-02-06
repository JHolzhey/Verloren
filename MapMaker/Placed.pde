class Placed {
  
  String name;
  PImage texture;
  float x, y;
  PlacableType type;
  int num_tiles_width, num_tiles_height;
  
  Placed(String name, PImage texture, float x, float y, PlacableType type, int num_tiles_width, int num_tiles_height) {
    this.name = name;
    this.x = x;
    this.y = y;
    this.type = type;
    this.num_tiles_width = num_tiles_width;
    this.num_tiles_height = num_tiles_height;
    
    int textureWidth = (int) (num_tiles_width * tileSize * 0.9f);
    int textureHeight = (int) (num_tiles_height * tileSize * 0.9f);
    this.texture = createImage(textureWidth, textureHeight, ARGB);
    this.texture.copy(texture, 0, 0, texture.width, texture.height, 0, 0, textureWidth, textureHeight);
  }
  
}
