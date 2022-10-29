////Column Strips [Bottom, Top]
//int Col1[2] = {16,0}; H->L
//int Col2[2] = {66,50}; H->L
//int Col3[2] = {67,83}; L->H <- Inverted!
//int Col4[2] = {100,84}; H->L
//int Col5[2] = {25,41}; L->H
//
////Row Strips [Left, Right]
//int RowB[2] = {17,24};
//int RowT[2] = {49,42};

void drawEdges(){
  
  for(int i = 0; i < NUMPIXELS; i++){
    if (i % 3 == 0){
      strip.setPixelColor(i, YELLOW);
    } else {
      strip.setPixelColor(i, BLUE);
    }
  }

//  if(nowTime - lastTime > 1000){
//    dowop = !dowop;
//  }
//
//  // Reset the timer
//  lastTime = nowTime;
//  nowTime = millis();
}
