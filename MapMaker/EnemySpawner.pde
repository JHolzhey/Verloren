class EnemySpawner extends Placed {
  
  // Lists the name of the enemy that will spawn in the (index + 1) wave
  ArrayList<Placable> enemies;
  
  EnemySpawner(String name, PImage texture, float x, float y) {
    super(name, texture, x, y, PlacableType.ENEMY_SPAWNER, 1, 1);
    enemies = new ArrayList<Placable>();
  }
  
}
