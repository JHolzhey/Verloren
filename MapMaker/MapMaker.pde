// MAP MAKER
import java.util.Iterator;

private final int TEXT_SIZE = 32;
private final int MAX_DIM_SIZE = 800;
private final String FILE_TO_LOAD_ROOM_FROM = "room_to_load.json";
private final String FILE_TO_SAVE_ROOM_TO = "generated_room.json";
private final boolean LOAD_ROOM_FROM_FILE = true;

private int numRows = 11;
private int numCols = 11;
private float tileSize;

// Currently selected placable object
private int curPlacableInd;

private ArrayList<Placable> placables;
private ArrayList<Placed> placedObjects;

enum PlacableType {
  CLEAR,
  OBSTACLE,
  ENEMY_SPAWNER,
  ENEMY,
  PLAYER_SPAWN,
  DOOR
}



void settings() {
  JSONObject roomJSON = null;
  // First load room width and height to be able to properly initialize placables (and their texture size)
  if (LOAD_ROOM_FROM_FILE) {
    try {
       roomJSON = loadJSONObject(FILE_TO_LOAD_ROOM_FROM);
      
      // Set grid size
      JSONObject gridSize = roomJSON.getJSONObject("grid_size");
      numRows = gridSize.getInt("num_rows");
      numCols = gridSize.getInt("num_cols");
    } catch (Exception e) {
      println("Error: Could not load json file: " + e);
    }
  }
  
  if (numRows > numCols) {
    tileSize = MAX_DIM_SIZE/numRows;
  } else {
    tileSize = MAX_DIM_SIZE/numCols;
  }
  
  placables = new ArrayList<Placable>();
  placedObjects = new ArrayList<Placed>();
  
  // Initializing all placable objects
  Placable clearPlacable = new Placable("clear", "cross.png", PlacableType.CLEAR, BACKSPACE);
  Placable enemy_spawner = new Placable("enemy spawner", "skull.png", PlacableType.ENEMY_SPAWNER, '1');
  Placable tree = new Placable("tree", "tree.png", PlacableType.OBSTACLE, '2', 1, 1);
  Placable exit = new Placable("exit", "door.png", PlacableType.DOOR, '3');
  Placable playerSpawn = new Placable("player spawn", "playerSpawn.png", PlacableType.PLAYER_SPAWN, '4');
  
  Placable passEnemy = new Placable("skip", "skip.png", PlacableType.ENEMY, 'p');
  Placable ratEnemy = new Placable("rat", "rat.png", PlacableType.ENEMY, 'r');
  Placable squirrelEnemy = new Placable("squirrel", "squirrel.png", PlacableType.ENEMY, 's');
  Placable wormEnemy = new Placable("worm", "worm.png", PlacableType.ENEMY, 'w');
  Placable bearEnemy = new Placable("bear", "bear.png", PlacableType.ENEMY, 'b');
  Placable deerEnemy = new Placable("deer", "deer.png", PlacableType.ENEMY, 'd');
  Placable foxEnemy = new Placable("fox", "fox.png", PlacableType.ENEMY, 'f');
  Placable boarEnemy = new Placable("boar", "boar.png", PlacableType.ENEMY, 'q');
  Placable rabbitEnemy = new Placable("rabbit", "rabbit.png", PlacableType.ENEMY, 'a');
  Placable wolfEnemy = new Placable("wolf", "wolf.png", PlacableType.ENEMY, 'o');
  
 
  placables.add(clearPlacable);
  placables.add(enemy_spawner);
  placables.add(tree);
  placables.add(exit);
  placables.add(playerSpawn);
  
  placables.add(passEnemy);
  placables.add(ratEnemy);
  placables.add(squirrelEnemy);
  placables.add(wormEnemy);
  placables.add(bearEnemy);
  placables.add(deerEnemy);
  placables.add(boarEnemy);
  placables.add(foxEnemy);
  placables.add(rabbitEnemy);
  placables.add(wolfEnemy);
  
  // Continue loading room once placables have been initialized
  if (LOAD_ROOM_FROM_FILE && roomJSON != null) {
    try {
      // Set doors
      JSONObject doorsJSON = roomJSON.getJSONObject("doors");
      JSONArray exitsArrJSON = doorsJSON.getJSONArray("exit");
      for (int i = 0; i < exitsArrJSON.size(); i++) {
        curPlacableInd = placables.indexOf(exit);
        JSONObject exitJSON = exitsArrJSON.getJSONObject(i);
        placeObject(exitJSON.getInt("x"), exitJSON.getInt("y"));
      }
      
      // Set obstacles
      JSONObject obstaclesJSON = roomJSON.getJSONObject("obstacles");
      JSONObject treeJSON = obstaclesJSON.getJSONObject("tree");
      if (treeJSON != null) {
        JSONArray treesPosArrJSON = treeJSON.getJSONArray("pos");
        for (int i = 0; i < treesPosArrJSON.size(); i++) {
          curPlacableInd = placables.indexOf(tree);
          JSONObject treePosJSON = treesPosArrJSON.getJSONObject(i);
          placeObject(treePosJSON.getInt("x"), treePosJSON.getInt("y"));
        }
      }
      // Set enemy spawners
      JSONArray enemySpawnersJSON = roomJSON.getJSONArray("enemy_spawners");
      for (int i = 0; i < enemySpawnersJSON.size(); i++) {
        curPlacableInd = placables.indexOf(enemy_spawner);
        JSONObject spawnerJSON = enemySpawnersJSON.getJSONObject(i);
        int spawnX = spawnerJSON.getInt("x");
        int spawnY = spawnerJSON.getInt("y");
        placeObject(spawnX, spawnY);
        
        JSONArray enemiesJSON = spawnerJSON.getJSONArray("enemies");
        for (int j = 0; j < enemiesJSON.size(); j++) {
          for (int k = 0; k < placables.size(); k++) {
            if (placables.get(k).name.equals(enemiesJSON.getString(j))) {
              curPlacableInd = placables.indexOf(placables.get(k));
              placeObject(spawnX, spawnY);
              break;
            }
          }
        }
      }
      
      // Set player spawn
      JSONObject playerSpawnJSON = roomJSON.getJSONObject("player_spawn");
      curPlacableInd = placables.indexOf(playerSpawn);
      placeObject(playerSpawnJSON.getInt("x"), playerSpawnJSON.getInt("y"));
    } catch (Exception e) {
      println("Error: Could not load room from JSON file: " + e);
    }
  }
  
  curPlacableInd = -1;
  
  size((int) (tileSize * numCols), (int) (tileSize * numRows));
}

void setup() {
  colorMode(HSB, 100);
  textAlign(CENTER);
  textSize(TEXT_SIZE);
}

void draw() {
  background(35, 70, 60);

  drawGrid();
  drawPlaced();
  drawSelectedTileOutline();
  showCurPlacable();
  
  drawUI();
}

void mousePressed() {
  //println(ceil(mouseX/tile_width) + " " + ceil(mouseY/tile_height));
  float xPos = floor(mouseX/tileSize);
  float yPos = floor(mouseY/tileSize);
  placeObject(xPos, yPos);
}

void mouseDragged() {
  if (curPlacableInd >= 0 && placables.get(curPlacableInd).type == PlacableType.ENEMY) {
    return;
  }
  
  float xPos = floor(mouseX/tileSize);
  float yPos = floor(mouseY/tileSize);
  placeObject(xPos, yPos);
}

void keyPressed() {
  // Enter key
  if (key == ENTER) {
    printMapJSON();
  } else if (key == '`') {
    curPlacableInd = -1;
  } else {
    for (Placable placable : placables) {
      if (key == placable.assigned_key) {
        curPlacableInd = placables.indexOf(placable);
        break;
      }
    }
  }
}

void drawGrid() {
  strokeWeight(1f);
  stroke(20);
  for (int i = 0; i < numRows; i++) {
    line(0, i * tileSize, width, i * tileSize);
  }
  for (int i = 0; i < numCols; i++) {
    line(i * tileSize, 0, i * tileSize, height);
  }
}

void drawSelectedTileOutline() {
  if (curPlacableInd != -1) {
    Placable curPlacable = placables.get(curPlacableInd);
    
    int xPos = floor(mouseX/tileSize);
    int yPos = floor(mouseY/tileSize);
    
    strokeWeight(4f);
    if (isValidPlacement(curPlacable, xPos, yPos)) {
      stroke(30, 100, 100);
    } else {
      stroke(0, 100, 100);
    }
    
    final float outlineTileRatio = 0.3;
    
    // Top left corner
    line(xPos * tileSize, yPos * tileSize, (xPos + outlineTileRatio) * tileSize, yPos * tileSize);
    line(xPos * tileSize, yPos * tileSize, xPos * tileSize, (yPos + outlineTileRatio) * tileSize);
    
    // Top right corner
    line((xPos + curPlacable.num_tiles_width) * tileSize, yPos * tileSize, (xPos + curPlacable.num_tiles_width - outlineTileRatio) * tileSize, yPos * tileSize);
    line((xPos + curPlacable.num_tiles_width) * tileSize, yPos * tileSize, (xPos + curPlacable.num_tiles_width) * tileSize, (yPos + outlineTileRatio) * tileSize);
    
    // Bottom left corner
    line(xPos * tileSize, (yPos + curPlacable.num_tiles_height) * tileSize, (xPos + outlineTileRatio) * tileSize, (yPos + curPlacable.num_tiles_height) * tileSize);
    line(xPos * tileSize, (yPos + curPlacable.num_tiles_height) * tileSize, xPos * tileSize, (yPos + curPlacable.num_tiles_height - outlineTileRatio) * tileSize);
    
    // Bottom right corner
    line((xPos + curPlacable.num_tiles_width) * tileSize, (yPos + curPlacable.num_tiles_height) * tileSize, (xPos + curPlacable.num_tiles_width - outlineTileRatio) * tileSize, (yPos + curPlacable.num_tiles_height) * tileSize);
    line((xPos + curPlacable.num_tiles_width) * tileSize, (yPos + curPlacable.num_tiles_height) * tileSize, (xPos + curPlacable.num_tiles_width) * tileSize, (yPos + curPlacable.num_tiles_height - outlineTileRatio) * tileSize);
  }
}

void drawPlaced() {
  for (Placed placed : placedObjects) {
    float xPos = (placed.x + placed.num_tiles_width * 0.05) * tileSize;
    float yPos = (placed.y + placed.num_tiles_height * 0.05) * tileSize;
    image(placed.texture, xPos, yPos);
    
    if (placed.type == PlacableType.ENEMY_SPAWNER) {
      EnemySpawner spawner = (EnemySpawner) placed;
      int num_enemies = spawner.enemies.size();
      float enemy_icon_max_width = tileSize/num_enemies;
      float enemy_icon_max_height = tileSize;
      final float min_icon_size = 25.f;
      int enemy_icon_size = (int) max(min(enemy_icon_max_width,enemy_icon_max_height), min_icon_size);
      // Rows and cols for displaying enemy icons
      int num_cols = floor(tileSize/ enemy_icon_size);
      int num_rows = ceil((float)num_enemies/(float)num_cols);
            
      outer:
      for (int i = 0; i < num_rows; i++) {
        for (int j = 0; j < num_cols; j++) {
          if (i * num_cols + j >= num_enemies) {
            break outer;
          }
          Placable enemy = spawner.enemies.get(i * num_cols + j);
          PImage enemyTexture = enemy.texture.copy();
          enemyTexture.resize(enemy_icon_size, enemy_icon_size);
          
          float icon_x = placed.x * tileSize + (j * enemy_icon_size);
          float icon_y = placed.y * tileSize + i * enemy_icon_size;
          
          image(enemyTexture, icon_x, icon_y);
        }
      }
    }
  }
}

// xPos and yPos are grid positions
boolean isValidPlacement(Placable placable, int xPos, int yPos) {
  if ((xPos + placable.num_tiles_width > numCols) ||
      (yPos + placable.num_tiles_height > numRows)) {
    return false;
  }
  
  Placable curPlacable = placables.get(curPlacableInd);
  for (Placed placedObject : placedObjects) {
    if (placedObject.name.equals(curPlacable.name) &&
        placedObject.x == xPos &&
        placedObject.y == yPos) {
      // Already found same obstacle placed there
      return false;
    }
    
    // Dont allow obstacles to overlap
    if (placable.type == PlacableType.OBSTACLE || placedObject.type == PlacableType.OBSTACLE) {
      if (xPos + placable.num_tiles_width > placedObject.x && xPos < placedObject.x + placedObject.num_tiles_width &&
          yPos + placable.num_tiles_height > placedObject.y && yPos < placedObject.y + placedObject.num_tiles_height) {
         return false;
      }
    }
  }
  return true;
}

void showCurPlacable() {
  if (curPlacableInd != -1) {
    Placable curPlacable = placables.get(curPlacableInd);
    
    float xPos = mouseX - (curPlacable.texture.width/2);
    float yPos = mouseY - (curPlacable.texture.height/2);
    image(curPlacable.texture, xPos,yPos);
  }
}

void drawUI() {
  float scale = 1f/18f;
  
  fill(17, 25, 100);
  strokeWeight(4);
  stroke(12, 68, 25);
  ellipse(width/2, height - width * scale, width * scale * 1.2, width * scale * 1.2);
  if (curPlacableInd != -1) {
    Placable curPlacable = placables.get(curPlacableInd);
    PImage curPlacableUI = createImage((int) (width * scale), (int) (width * scale), ARGB);
    curPlacableUI.copy(curPlacable.texture, 0, 0, 
                                            curPlacable.texture.width, curPlacable.texture.height,
                                            0, 0,
                                            (int) (width * scale), (int) (width * scale));
    image(curPlacableUI, width/2 - curPlacableUI.width/2, height - width * scale - curPlacableUI.height/2);
    
    fill(100);
    text(curPlacable.name, width/2, (int) (height - width * scale - width * scale + TEXT_SIZE/2));
  }
  
  
}

void placeObject(float xPos, float yPos) {
  if (curPlacableInd >= 0) {
    Placable curPlacable = placables.get(curPlacableInd);
    
    // Trying to clear the current tile
    if (curPlacable.type == PlacableType.CLEAR) {
      Iterator<Placed> itr = placedObjects.iterator();
      while (itr.hasNext()) {
        Placed placedObject = itr.next();
        if (placedObject.x == xPos &&
            placedObject.y == yPos) {
          itr.remove();
        }
      }
    } 
    // Trying to place enemy on spawners
    else if (curPlacable.type == PlacableType.ENEMY) {
      for (Placed placedObject : placedObjects) {
        if (placedObject.type == PlacableType.ENEMY_SPAWNER &&
            placedObject.x == xPos && 
            placedObject.y == yPos) {
              EnemySpawner enemySpawner = (EnemySpawner) placedObject;
              enemySpawner.enemies.add(curPlacable);
          return;
        }
      }
    } else {      
      if (!isValidPlacement(curPlacable, (int) xPos, (int) yPos)) {
        return;
      }
      
      if (curPlacable.type == PlacableType.ENEMY_SPAWNER) {
        EnemySpawner enemySpawner = new EnemySpawner(curPlacable.name, curPlacable.texture, xPos, yPos);
        placedObjects.add(enemySpawner);
      } else {
        Placed newlyPlaced = new Placed(curPlacable.name, curPlacable.texture, xPos, yPos, curPlacable.type, curPlacable.num_tiles_width, curPlacable.num_tiles_height);
        placedObjects.add(newlyPlaced);
      }
    }
  }
}

void printMapJSON() {
  JSONObject mapJSON = new JSONObject();
  
  JSONObject grid_size = new JSONObject();
  grid_size.put("num_rows", numRows);
  grid_size.put("num_cols", numCols);
  
  JSONObject obstacles = new JSONObject();
  JSONObject doors = new JSONObject();
  JSONArray enemySpawners = new JSONArray();

  for (Placed placedObject : placedObjects) {
    switch (placedObject.type) {
      case OBSTACLE:
        addPlacedObjectToJSONWithSize(placedObject, obstacles);
        break;
      case DOOR:
        addPlacedObjectToJSON(placedObject, doors);
        break;
      case ENEMY_SPAWNER:
        EnemySpawner spawner = (EnemySpawner) placedObject;
          
        JSONObject spawner_data = new JSONObject();
        spawner_data.put("x", spawner.x);
        spawner_data.put("y", spawner.y);
        
        JSONArray spawner_enemies = new JSONArray();
        for (Placable contained_enemy : spawner.enemies) {
          spawner_enemies.append(contained_enemy.name);
        }
        spawner_data.put("enemies", spawner_enemies);
        
        enemySpawners.append(spawner_data);
        break;
      case PLAYER_SPAWN:
        JSONObject playerSpawn = new JSONObject();
        playerSpawn.put("x", placedObject.x);
        playerSpawn.put("y", placedObject.y);
        mapJSON.put("player_spawn", playerSpawn);
        break;
      default:
        println("ERROR - Unimplemented placed object type: " + placedObject.type);
    }
  }
  
  mapJSON.put("grid_size", grid_size);
  mapJSON.put("obstacles", obstacles);
  mapJSON.put("doors", doors);
  mapJSON.put("enemy_spawners", enemySpawners);
  if (mapJSON.get("player_spawn") == null) {
    JSONObject defaultPlayerSpawn = new JSONObject();
    defaultPlayerSpawn.put("x", 0);
    defaultPlayerSpawn.put("y", 0);
    mapJSON.put("player_spawn", defaultPlayerSpawn);
  }
  
  try {
    saveJSONObject(mapJSON, FILE_TO_SAVE_ROOM_TO);
    println("Saved room!");
  } catch (Exception e) {
    println("Error: Could not save room to JSON file: " + e);
    println("ROOM JSON: ");
    println(mapJSON);
  }
}

void addPlacedObjectToJSONWithSize(Placed placedObject, JSONObject targetJSON) {
  JSONObject objectData = targetJSON.getJSONObject(placedObject.name);
  if (objectData == null) {
    objectData = new JSONObject();
    targetJSON.setJSONObject(placedObject.name, objectData);
    objectData.setJSONArray("pos", new JSONArray());
    JSONObject size = new JSONObject();
    size.put("w", placedObject.num_tiles_width);
    size.put("h", placedObject.num_tiles_height);
    objectData.setJSONObject("size", size);
  }
  JSONArray objectPosArr = objectData.getJSONArray("pos");
  
  JSONObject objectPos = new JSONObject();
  objectPos.put("x", placedObject.x);
  objectPos.put("y", placedObject.y);
  objectPosArr.append(objectPos);
}

void addPlacedObjectToJSON(Placed placedObject, JSONObject targetJSON) {
  JSONArray objectArr = targetJSON.getJSONArray(placedObject.name);
      
  if (objectArr == null) {
    targetJSON.put(placedObject.name, new JSONArray());
    objectArr = (JSONArray) targetJSON.get(placedObject.name);
  }
  
  JSONObject pos = new JSONObject();
  pos.put("x", placedObject.x);
  pos.put("y", placedObject.y);
  objectArr.append(pos);
}
