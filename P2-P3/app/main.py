from fastapi import FastAPI, File, UploadFile
import numpy as np
import cv2
from datetime import datetime

app = FastAPI()

detectedFaces = []

@app.get("/")
def root():
    return {"message": "Hello World"}

faceCascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')

@app.post("/upload")
async def uploadImage(file: UploadFile = File(...)):
    content = await file.read()
    npArray = np.frombuffer(content, np.uint8)
    image = cv2.imdecode(npArray, cv2.IMREAD_COLOR)
    grayImage = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    faces = faceCascade.detectMultiScale(grayImage, scaleFactor=1.1, minNeighbors=5, minSize=(30, 30))

    detectedFaces = [(x, y, w, h) for (x, y, w, h) in faces]
    
    for (x, y, w, h) in faces:
        cv2.rectangle(image, (x, y), (x + w, y + h), (255, 0, 0), 2)
    
    timestamp = datetime.now().strftime("%d%m%Y%H%M%S")
    filename = f"/app/images/receivedImage-{timestamp}.jpg"
    cv2.imwrite(filename, image)
    
    return {"message": f"Image received and saved as {filename}"}

@app.get("/get_faces")
def getFaces():
    return {"faces": detectedFaces}

if __name__ == '__main__':
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000, debug=True)