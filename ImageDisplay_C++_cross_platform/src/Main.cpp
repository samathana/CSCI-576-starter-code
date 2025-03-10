#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

/**
 * Display an image using WxWidgets.
 * https://www.wxwidgets.org/
 */

/** Declarations*/

/**
 * Class that implements wxApp
 */
class MyApp : public wxApp {
 public:
  bool OnInit() override;
};

/**
 * Class that implements wxFrame.
 * This frame serves as the top level window for the program
 */
class MyFrame : public wxFrame {
 public:
  MyFrame(const wxString &title, string imagePath);

 private:
  void OnPaint(wxPaintEvent &event);
  wxImage inImage;
  wxScrolledWindow *scrolledWindow;
  int width;
  int height;
};

/** Utility function to read image data */
unsigned char *readImageData(string imagePath, int width, int height);

/** Definitions */

/**
 * Init method for the app.
 * Here we process the command line arguments and
 * instantiate the frame.
 */
bool MyApp::OnInit() {
  wxInitAllImageHandlers();

  // deal with command line arguments here
  cout << "Number of command line arguments: " << wxApp::argc << endl;
  if (wxApp::argc != 2) {
    cerr << "The executable should be invoked with exactly three filepath "
            "arguments. Example ./MyImageApplication '../../Lena_512_512.rgb 2 4'"
         << endl;
    exit(1);
  }
  cout << "First argument: " << wxApp::argv[0] << endl;
  cout << "Second argument: " << wxApp::argv[1] << endl;
  string imagePath = wxApp::argv[1].ToStdString();

  MyFrame *frame = new MyFrame("Image Display", imagePath);
  frame->Show(true);

  // return true to continue, false to exit the application
  return true;
}

/**
 * Constructor for the MyFrame class.
 * Here we read the pixel data from the file and set up the scrollable window.
 */
MyFrame::MyFrame(const wxString &title, string imagePath)
    : wxFrame(NULL, wxID_ANY, title) {

  // Modify the height and width values here to read and display an image with
  // different dimensions.    
  width = 352;
  height = 288;

  unsigned char *inData = readImageData(imagePath, width, height);

  // the last argument is static_data, if it is false, after this call the
  // pointer to the data is owned by the wxImage object, which will be
  // responsible for deleting it. So this means that you should not delete the
  // data yourself.
  inImage.SetData(inData, 2 * width, height, false);

  // Set up the scrolled window as a child of this frame
  scrolledWindow = new wxScrolledWindow(this, wxID_ANY);
  scrolledWindow->SetScrollbars(10, 10, 2 * width, height);
  scrolledWindow->SetVirtualSize(2 * width, height);

  // Bind the paint event to the OnPaint function of the scrolled window
  scrolledWindow->Bind(wxEVT_PAINT, &MyFrame::OnPaint, this);

  // Set the frame size
  SetClientSize(width * 2, height);

  // Set the frame background color
  SetBackgroundColour(*wxBLACK);
}

/**
 * The OnPaint handler that paints the UI.
 * Here we paint the image pixels into the scrollable window.
 */
void MyFrame::OnPaint(wxPaintEvent &event) {
  wxBufferedPaintDC dc(scrolledWindow);
  scrolledWindow->DoPrepareDC(dc);

  wxBitmap inImageBitmap = wxBitmap(inImage);
  dc.DrawBitmap(inImageBitmap, 0, 0, false);
}

int findClosest(int x, int y, vector<pair<int, int>>& codebook) {
  int minDistance = 255;
  int nearestIndex = 0;
  for (int j = 0; j < codebook.size(); j++) { //find closest cluster to map to
    double distance = sqrt(pow(x - codebook[j].first, 2) + pow(y - codebook[j].second, 2));
    if (distance < minDistance) {
      minDistance = distance;
      nearestIndex = j;
    }
  }
  return nearestIndex;
}

/** Utility function to read image data */
unsigned char *readImageData(string imagePath, int width, int height) {

  // Open the file in binary mode
  ifstream inputFile(imagePath, ios::binary);

  if (!inputFile.is_open()) {
    cerr << "Error Opening File for Reading" << endl;
    exit(1);
  }

  // Create and populate RGB buffers
  vector<char> Rbuf(width * height);
  vector<char> Gbuf(width * height);
  vector<char> Bbuf(width * height);

  vector<char> newR(width * height);
  vector<char> newG(width * height);
  vector<char> newB(width * height);

  /**
   * The input RGB file is formatted as RRRR.....GGGG....BBBB.
   * i.e the R values of all the pixels followed by the G values
   * of all the pixels followed by the B values of all pixels.
   * Hence we read the data in that order.
   */

  inputFile.read(Rbuf.data(), width * height);
  inputFile.read(Gbuf.data(), width * height);
  inputFile.read(Bbuf.data(), width * height);

  inputFile.close();

  int n = 4;
  int sqrtN = sqrt(n);
  int intervalSize = 256/sqrtN;
  vector<pair<int, int>> codebook;
  vector<pair<int, int>> sumVecs(n, make_pair(0, 0)); //sum of vectors in entry
  vector<int> numVecs(codebook.size(), 0); //num vectors, initialized to 0
  for (int i = 0; i < sqrtN; i++) {
    for (int j = 0; j < sqrtN; j++) { //initial, uniform codebook
      codebook.push_back({i * intervalSize + (intervalSize / 2), j * intervalSize + (intervalSize / 2)});
    }
  }
  //codebook refinement:
  for (int j = 0; j < 10; j++) { //repeat 10 times
    sumVecs = vector<pair<int, int>>(codebook.size(), make_pair(0, 0)); //sum of inputs in an entry
    numVecs = vector<int>(codebook.size(), 0); //num inputs mapped to entry
    for (int i = 0; i < width * height - 1; i += 2) { //for every input:
      int x = (unsigned char) Rbuf[i];
      int y = (unsigned char) Rbuf[i + 1];
      int nearestIndex = 0;
      nearestIndex = findClosest(x, y, codebook);
      sumVecs[nearestIndex].first += x;
      sumVecs[nearestIndex].second += y;
      numVecs[nearestIndex]++;
    }
    for (int i = 0; i < codebook.size(); i++) { //set to new values
      if (numVecs[i] > 0) {
        codebook[i].first = sumVecs[i].first / numVecs[i];
        codebook[i].second = sumVecs[i].second / numVecs[i];
      }
    }
  }

  int closestIndex;
  for (int i = 0; i < width * height - 1; i += 2) {
    closestIndex = findClosest((int) (unsigned char) Rbuf[i], (int) (unsigned char) Rbuf[i + 1], codebook);
    newR[i] = codebook[closestIndex].first;
    newR[i + 1] = codebook[closestIndex].second;
    closestIndex = findClosest((int) (unsigned char) Gbuf[i], (int) (unsigned char) Gbuf[i + 1], codebook);
    newG[i] = codebook[closestIndex].first;
    newG[i + 1] = codebook[closestIndex].second;
    closestIndex = findClosest((int) (unsigned char) Bbuf[i], (int) (unsigned char) Bbuf[i + 1], codebook);
    newB[i] = codebook[closestIndex].first;
    newB[i + 1] = codebook[closestIndex].second;
  }
  
  /**
   * Allocate a buffer to store the pixel values
   * The data must be allocated with malloc(), NOT with operator new. wxWidgets
   * library requires this.
   */
  int newWidth = width * 2;
  unsigned char *inData =
      (unsigned char *)malloc(newWidth * height * 3 * sizeof(unsigned char));
      
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      // We populate RGB values of each pixel in that order
      // RGB.RGB.RGB and so on for all pixels
      inData[3 * (i + j * newWidth)] = Rbuf[i + j * width];
      inData[3 * (i + j * newWidth) + 1] = Gbuf[i + j * width];
      inData[3 * (i + j * newWidth) + 2] = Bbuf[i + j * width];
    }
    for (int i = width; i < width * 2; i++) {
      // side by side
      inData[3 * (i + j * newWidth)] = newR[i + j * width];
      inData[3 * (i + j * newWidth) + 1] = newG[i + j * width];
      inData[3 * (i + j * newWidth) + 2] = newB[i + j * width];
    }
  }

  return inData;
}

wxIMPLEMENT_APP(MyApp);