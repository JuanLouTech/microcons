#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

/*
MEMORY UTILITIES
*/
void writeIntIntoEEPROM(int address, int number)
{ 
  EEPROM.write(address, number >> 8);
  EEPROM.write(address + 1, number & 0xFF);
}
int readIntFromEEPROM(int address)
{
  return (EEPROM.read(address) << 8) + EEPROM.read(address + 1);
}
/*
--------------------------------
*/

String text;
int width = 128;
int height = 64;

Adafruit_SSD1306 display(width, height, &Wire, -1);

bool screenReady = true;
int errorLed = 15;
int leftButton = 5;
int rightButton = 4;
int dPad = 20;

struct InputMapping {
  float up;
  float down;
  float left;
  float right;
  float enter;
};

InputMapping controls = {3.0, 50.0, 2.0, 6.0, 1.0};

int verticalLimits[] = {0, 64};
int horizontalLimits[] = {0, 128};
int playerSize[] = {8,9};
int playerPosition[] = {60,64 - playerSize[1]};
int moveSpeed = 2;
int verticalVelocity = 0;
int gravity = 1;
int jumpForce = 8;
int playerDirection = 1;
bool canJump = true;

int npcPosition[] = {60, 4};
int npcSize[] = {8,9};
int npcMoveSpeed = 4;
int npcDirection = 1;
unsigned long lastNpcMoveTime = 0;
unsigned long timeForNextNpcMove = 1000;
int npcPlatformHeight = 14;

int enemyPosition[] = {0, 0};
int enemyRadius = 5;
int enemySpeed = 3;
int enemyDirection = 1;
int enemyBounces = 3;
bool enemyMouthOpen = false;
int enemyMouthSize[] = {2, 4};
unsigned long enemyMouthOpenTime = 500;
bool isEnemyActive = false;
unsigned long lastEnemyActiveTime = 0;
unsigned long timeForNextEnemyActive = 5000;


bool isFruitActive = false;
int fruitRadius = 2;
int fruitPosition[] = {0, 0};
int fruitSpeed = 2;
unsigned long lastFruitTime = 0;
unsigned long timeForNextFruit = 4000;

int platformLeftSize[] = {10,3};
int platformLeftPosition[] = {0, height - 15};

int platformRightSize[] = {10,3};
int platformRightPosition[] = {width - platformRightSize[0], height - 15};

int score = 0;
bool isGameOver = false;
int highScore = 0;
int highScoreAddress = 45;

void resetGame();

void setup() {
  Serial.begin(115200);
  pinMode(leftButton, INPUT_PULLUP);
  pinMode(rightButton, INPUT_PULLUP);
  pinMode(errorLed, OUTPUT);
  pinMode(dPad, INPUT);


  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    screenReady = false;
    digitalWrite(errorLed, HIGH);
    return;
  }
  resetGame();

  display.clearDisplay();
}

void resetGame() {
  score = 0;
  playerPosition[0] = 60;
  playerPosition[1] = 64 - playerSize[1];
  npcPosition[0] = 60;
  npcPosition[1] = 4;
  isFruitActive = false;
  isEnemyActive = false;
  lastEnemyActiveTime = millis();
  timeForNextEnemyActive = random(4000, 10000);
  isGameOver = false;
  highScore = readIntFromEEPROM(highScoreAddress);
}

/*
COLLISION DETECTION
*/
bool isPlayerTouchingFruit() {
  if (!isFruitActive) return false;
  int fruitCenterX = fruitPosition[0];
  int fruitCenterY = fruitPosition[1];
  int playerCenterX = playerPosition[0] + playerSize[0]/2;
  int playerCenterY = playerPosition[1] + playerSize[1]/2;
  int distance = sqrt(pow(fruitCenterX - playerCenterX, 2) + pow(fruitCenterY - playerCenterY, 2));
  return distance < fruitRadius + playerSize[1]/2;
}

bool isPlayerStandingOnAnyPlatform() {
  if ((playerPosition[0] > platformLeftPosition[0] + platformLeftSize[0] &&
      playerPosition[0] + playerSize[0] < platformRightPosition[0]) || 
      playerPosition[1] + playerSize[1] < platformLeftPosition[1]) {
    return false;
  }
  // Check if the player is standing at the platform height
  if (playerPosition[1] + playerSize[1] >= platformLeftPosition[1] && 
    playerPosition[1] + playerSize[1] <= platformLeftPosition[1] + platformLeftSize[1]) 
    return true;
  
  return false;
}

bool isPlayerTouchingEnemy() {
  if (!isEnemyActive) return false;
  int enemyCenterX = enemyPosition[0] + enemyRadius;
  int enemyCenterY = enemyPosition[1] + enemyRadius;
  int playerCenterX = playerPosition[0] + playerSize[0]/2;
  int playerCenterY = playerPosition[1] + playerSize[1]/2;
  int distance = sqrt(pow(enemyCenterX - playerCenterX, 2) + pow(enemyCenterY - playerCenterY, 2));
  return distance < enemyRadius + playerSize[0]/2;
}

/*
FRUIT CREATION
*/

void createFruit() {
  fruitPosition[0] = npcPosition[0] + npcSize[0]/2;
  fruitPosition[1] = npcPosition[1] + npcSize[1] - npcSize[1]/4;
  isFruitActive = true;
}

/*
ENEMY CREATION
*/
void createEnemy() {
  isEnemyActive = true;
  enemyDirection = random(0, 2) == 0 ? -1 : 1;
  enemyPosition[0] = enemyDirection == 1 ? 0 - enemyRadius : width;
  enemyPosition[1] = height - enemyRadius * 2;
  enemyBounces = random(1, 4);
}

/* 
INPUT READING AND PLAYER MOVEMENT 
*/
int input() {
  int dPadInput = analogRead(dPad);
  // This cleans and stabilize the input from the dPad.
  return ceil(map(dPadInput, 0, 1023, 0, 100)/2.0);
}

void getInput()
{
  int movementX = 0;
  
  if (playerPosition[1] + playerSize[1] == verticalLimits[1] || (isPlayerStandingOnAnyPlatform() && verticalVelocity == 0)) {
    canJump = true;
  }

  if (input() == float(controls.left) || input() == float(controls.left + controls.up)) {
    movementX = -moveSpeed;
    playerDirection = -1;
  }
  if (input() == float(controls.right)|| input() == float(controls.right + controls.up)) {
    movementX =  moveSpeed;
    playerDirection = 1;
  }
  if ((input() == float(controls.up) ||
   input() == float(controls.right + controls.up) ||
   input() == float(controls.left + controls.up)) && canJump) {
    verticalVelocity = -jumpForce;
    canJump = false;
  }
  
  int newX = playerPosition[0] + movementX;
  if (newX < horizontalLimits[0]) newX = horizontalLimits[0];
  if (newX > horizontalLimits[1] - playerSize[0]) newX = horizontalLimits[1] - playerSize[0];
   
    
  int newY = playerPosition[1];
  if (playerPosition[1] + playerSize[1] <= verticalLimits[1]) {
      verticalVelocity += gravity;
      newY = playerPosition[1] + verticalVelocity;
  }   
  else {
    verticalVelocity = 0;
  }
  verticalVelocity = newY <= npcPlatformHeight? 0 : verticalVelocity;
  if (newY < verticalLimits[0]) newY = verticalLimits[0];
  if (isPlayerStandingOnAnyPlatform() && verticalVelocity > 0) {
    newY = platformLeftPosition[1] - playerSize[1];
    verticalVelocity = 0;
  }

  if (newY > verticalLimits[1] - playerSize[1]) newY = verticalLimits[1] - playerSize[1];
  playerPosition[0] = newX;
  playerPosition[1] = newY;
}

/* 
REST OF MOVEMENT 
*/
void moveNPC()
{
  if (millis() >= lastNpcMoveTime + timeForNextNpcMove) {
    lastNpcMoveTime = millis();
    timeForNextNpcMove = random(1000, 4000);
    npcDirection = random(0, 2) == 0 ? -1 : 1;
  }
  npcPosition[0] += npcDirection * npcMoveSpeed;
  if (npcPosition[0] < horizontalLimits[0]) {
    npcPosition[0] = horizontalLimits[0];
    npcDirection *= -1;
  }
  if (npcPosition[0] > horizontalLimits[1] - npcSize[0]) {
    npcPosition[0] = horizontalLimits[1] - npcSize[0];
    npcDirection *= -1;
  }
}

void moveFruit() {
  if (!isFruitActive) return;
  fruitPosition[1] += fruitSpeed;
  if (fruitPosition[1] > height) {
    isFruitActive = false;
  }
}

void moveEnemy() {
  if (!isEnemyActive) return;
  if (millis() >= enemyMouthOpenTime + 200) {
    enemyMouthOpen = !enemyMouthOpen;
    enemyMouthOpenTime = millis();
  }

  enemyPosition[0] += enemyDirection * enemySpeed;

  if (enemyBounces <= 0) {
    if (enemyPosition[0] < horizontalLimits[0] || enemyPosition[0] > horizontalLimits[1] + enemyRadius)
      isEnemyActive = false;
      lastEnemyActiveTime = millis();
      timeForNextEnemyActive = random(3000, 6000);
    return;
  }

  if (enemyPosition[0] < horizontalLimits[0] && enemyDirection == -1) {
    enemyPosition[0] = horizontalLimits[0];
    enemyDirection *= -1;
    enemyBounces--;
  }

  if (enemyPosition[0] > horizontalLimits[1] - enemyRadius * 2 && enemyDirection == 1) {
    enemyPosition[0] = horizontalLimits[1] - enemyRadius * 2;
    enemyDirection *= -1;
    enemyBounces--;
  }
}
/* 
PAINT STUFF FUNCTIONS
*/
void drawGhost(int  position[2], int size[2], int direction) {
   // Body (x: 0, y: 2, size: 8x6)
  display.fillRect(position[0], position[1] + 2, size[0], size[1] - 2, SSD1306_WHITE);
  // Head 1 (x: 1, y: 1, size: 6x1)
  display.fillRect(position[0] + 1, position[1] + 1, size[0] - 2, 1, SSD1306_WHITE);
  // Head 2 (x: 0, y: 2, size: 4x1)
  display.fillRect(position[0] + 2, position[1], size[0] - 4, 1, SSD1306_WHITE);

  int faceXOffset = direction == 1 ? 3 : 0;
  // Left eye (x: offset, y: 3, size: 2x2)
  display.fillRect(position[0] + faceXOffset, position[1] + 3, 2, 2, SSD1306_BLACK);
  // Right eye (x: offset + 2, y: 3, size: 2x2)
  display.fillRect(position[0] + faceXOffset + 3, position[1] + 3, 2, 2, SSD1306_BLACK);
  // Base cut 1 (x: offset - 1, y: size[0] - 1, size: 1x1)
  display.fillRect(position[0] + faceXOffset - 1, position[1] + size[1] - 1, 1, 1, SSD1306_BLACK);
  // Base cut 2 (x: offset + 2, y: size[0] - 1, size: 1x1)
  display.fillRect(position[0] + faceXOffset + 2, position[1] + size[1] - 1, 1, 1, SSD1306_BLACK);
  // Base cut 3 (x: offset + 4, y: size[0] - 1, size: 1x1)
  display.fillRect(position[0] + faceXOffset + 4, position[1] + size[1] - 1, 1, 1, SSD1306_BLACK);
}


void drawPlayer()
{
  drawGhost(playerPosition, playerSize, playerDirection);
}

void drawNPC()
{
  drawGhost(npcPosition, npcSize, npcDirection);
}


void drawFruit()
{
  if (!isFruitActive) return;
  display.fillCircle(fruitPosition[0], fruitPosition[1], fruitRadius, SSD1306_WHITE);
}

void drawEnemy() {
  if (!isEnemyActive) return;
  // Enemy looks like packman, with one visible eye and the mouth open and closes
  // Body
  display.fillCircle(enemyPosition[0] + enemyRadius, enemyPosition[1] + enemyRadius, enemyRadius, SSD1306_WHITE);
  // Eye (depending on the direction of moving)
  int eyeOffset = enemyDirection == 1 ? 1 : -3;
  display.fillRect(enemyPosition[0] + enemyRadius + eyeOffset, enemyPosition[1] + enemyRadius - 2, 2, 2, SSD1306_BLACK);

  if (!enemyMouthOpen) return;
  int mouthOffset = enemyDirection == 1 ? enemyRadius + 3 : 0;
  display.fillRect(enemyPosition[0] + mouthOffset, enemyPosition[1] + enemyRadius - 2, 2, 4, SSD1306_BLACK);
  display.drawLine(enemyPosition[0] + mouthOffset + 2, enemyPosition[1] + enemyRadius, enemyPosition[0] + mouthOffset + 2, enemyPosition[1] + enemyRadius + 4, SSD1306_BLACK);
}

void drawPlatforms()
{
  display.drawRect(platformLeftPosition[0], platformLeftPosition[1], platformLeftSize[0], platformLeftSize[1], SSD1306_WHITE);
  display.drawRect(platformRightPosition[0], platformRightPosition[1], platformRightSize[0], platformRightSize[1], SSD1306_WHITE);
}

void drawWorld()
{
  display.drawLine(0, npcPlatformHeight, width, npcPlatformHeight, SSD1306_WHITE);
  drawPlatforms();
}


/*
GAME OVER
*/
void gameOver() {
  isGameOver = true;
  display.clearDisplay();
  text = "Game Over!";
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(text);
  text = score > highScore? "New High Score: " + String(score) : "Score: " + String(score);
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println(text);
  text = "Press JUMP to retry";
  display.setTextSize(1);
  display.setCursor(0, 50);
  display.println(text);
  display.display();
  if (score > highScore) {
    highScore = score;
    writeIntIntoEEPROM(highScoreAddress, highScore);
  }
}

/* 
MAIN LOOP
*/
void loop() {
  if (!screenReady) {
    return;
  }
  if (isGameOver) {
    if (input() == float(controls.up)) {
      resetGame();
    }
    return;
  } 

  if (isPlayerTouchingEnemy()) {
    gameOver();
    return;
  }

  getInput();
  moveNPC();
  
  if (millis() >= lastFruitTime + timeForNextFruit) {
    lastFruitTime = millis();
    timeForNextFruit = random(4000, 7000);
    createFruit();
  }
  
  moveFruit();

  if (!isEnemyActive && millis() >= lastEnemyActiveTime + timeForNextEnemyActive) {
    createEnemy();
  }

  moveEnemy();

  display.clearDisplay();
  drawPlayer();
  drawNPC();
  drawFruit();
  drawWorld();
  drawEnemy();
  if (isPlayerTouchingFruit()) {
    score++;
    isFruitActive = false;
  }
  text = "Score: " + String(score);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(text);
  text = "High: " + String(highScore);
  display.setCursor(width - 50, 0);
  display.println(text);
  display.display();
}