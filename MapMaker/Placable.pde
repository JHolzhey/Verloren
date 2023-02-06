class Placable {
  
  final String TEXTURE_PATH = "textures/";
  
  String name;
  PImage texture;
  PlacableType type;
  char assigned_key;
  int num_tiles_width, num_tiles_height;
  
  Placable(String name, String imgPath, PlacableType type, char assigned_key) {
    this(name, imgPath, type, assigned_key, 1, 1);
  }
  
  Placable(String name, String imgPath, PlacableType type, char assigned_key, int num_tiles_width, int num_tiles_height) {
    this.name = name;
    this.type = type;
    this.assigned_key = assigned_key;
    this.num_tiles_width = num_tiles_width;
    this.num_tiles_height = num_tiles_height;
    texture = loadImage(TEXTURE_PATH + imgPath);
    
    texture.resize((int) (tileSize * 0.9f), (int) (tileSize * 0.9f));
  }
}
