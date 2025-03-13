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
  MyFrame(const wxString &title, string imagePath, int m, int n);

 private:
  void OnPaint(wxPaintEvent &event);
  wxImage inImage;
  wxScrolledWindow *scrolledWindow;
  int width;
  int height;
};

/** Utility function to read image data */
unsigned char *readImageData(string imagePath, int width, int height, int m, int n);

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
  if (wxApp::argc != 4) {
    cerr << "The executable should be invoked with exactly three "
            "arguments. Example ./MyImageApplication '../../Lena_512_512.rgb 2 4'"
         << endl;
    exit(1);
  }
  cout << "First argument: " << wxApp::argv[0] << endl;
  cout << "Second argument: " << wxApp::argv[1] << endl;
  cout << "Third argument: " << wxApp::argv[2] << endl;
  cout << "Fourth argument: " << wxApp::argv[3] << endl;
  string imagePath = wxApp::argv[1].ToStdString();

  MyFrame *frame = new MyFrame("Image Display", imagePath, stoi((wxApp::argv[2]).ToStdString()), stoi((wxApp::argv[3]).ToStdString()));
  frame->Show(true);

  // return true to continue, false to exit the application
  return true;
}

/**
 * Constructor for the MyFrame class.
 * Here we read the pixel data from the file and set up the scrollable window.
 */
MyFrame::MyFrame(const wxString &title, string imagePath, int m, int n)
    : wxFrame(NULL, wxID_ANY, title) {

  // Modify the height and width values here to read and display an image with
  // different dimensions.    
  width = 352;
  height = 288;

  unsigned char *inData = readImageData(imagePath, width, height, m, n);

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

//for m = 2
int findClosestCombined(int r1, int r2, int g1, int g2, int b1, int b2, vector<pair<tuple<int, int, int>, tuple<int, int, int>>>& codebook, bool color) {
  int minDistance = 255;
  int nearestIndex = 0;
  for (int j = 0; j < codebook.size(); j++) { //find closest cluster to map to
    double distance = sqrt(pow(r1 - get<0>(codebook[j].first), 2) + pow(r2 - get<0>(codebook[j].second), 2));
    if (color) {
      double dg = sqrt(pow(g1 - get<1>(codebook[j].first), 2) + pow(g2 - get<1>(codebook[j].second), 2));
      double db = sqrt(pow(b1 - get<2>(codebook[j].first), 2) + pow(b2 - get<2>(codebook[j].second), 2));
      distance += dg + db;
    }
    if (distance < minDistance) {
      minDistance = distance;
      nearestIndex = j;
    }
  }
  return nearestIndex;
}

//for sq
int findClosestSq(vector<char>& Rbuf, vector<char>& Gbuf, vector<char>& Bbuf, int x, int y, vector<vector<vector<tuple<int, int, int>>>>& codebook, bool color, int width) {
  int minDistance = 255; 
  int nearestIndex = 0;
  for (int j = 0; j < codebook.size(); j++) {
    double distance = 0;
    for (int dy = 0; dy < codebook[j].size(); dy++) {
      for (int dx = 0; dx < codebook[j][dy].size(); dx++) {
        int index = (y + dy) * width + (x + dx);
        int rDiff = (int) (unsigned char) Rbuf[index] - get<0>(codebook[j][dy][dx]);
        distance += rDiff * rDiff;
        if (color) {
          int gDiff = (int) (unsigned char) Gbuf[index] - get<1>(codebook[j][dy][dx]);
          distance += gDiff * gDiff;
          int bDiff = (int) (unsigned char) Bbuf[index] - get<2>(codebook[j][dy][dx]);
          distance += bDiff * bDiff;
        }
      }
    }
    distance = sqrt(distance);
    if (distance < minDistance) {
      minDistance = distance;
      nearestIndex = j;
    }
  }
  return nearestIndex;
}

/** Utility function to read image data */
unsigned char *readImageData(string imagePath, int width, int height, int m, int n) {

  // Open the file in binary mode
  ifstream inputFile(imagePath, ios::binary);

  if (!inputFile.is_open()) {
    cerr << "Error Opening File for Reading" << endl;
    exit(1);
  }

  bool color = true;
  if (imagePath.find(".raw") != std::string::npos)
    color = false;

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

  if (m == 2) {
    //make initial codebook:
    vector<pair<tuple<int, int, int>, tuple<int, int, int>>> codebook;
    vector<pair<tuple<int, int, int>, tuple<int, int, int>>> sumVecs(n, {{0, 0, 0}, {0, 0, 0}}); //sum of vectors in entry
    vector<int> numVecs(codebook.size(), 0); //num vectors, initialized to 0
    for (int i = 0; i < n; i++) {
      codebook.push_back({{i * (256 / n), i * (256 / n), i * (256 / n)}, {i * (256 / n), i * (256 / n), i * (256 / n)}});
    }

    //codebook refinement:
    int curDiff = 255;
    int iter = 0;
    while (curDiff > 10 && iter++ < 50) { 
      sumVecs = vector<pair<tuple<int, int, int>, tuple<int, int, int>>>(n, {{0, 0, 0}, {0, 0, 0}}); //sum of inputs in an entry
      numVecs = vector<int>(codebook.size(), 0); //num inputs mapped to entry
      for (int i = 0; i < width * height - 1; i += 2) { //for every input:
        int r1 = (unsigned char) Rbuf[i];
        int r2 = (unsigned char) Rbuf[i + 1];
        int g1 = (unsigned char) Gbuf[i];
        int g2 = (unsigned char) Gbuf[i + 1];
        int b1 = (unsigned char) Bbuf[i];
        int b2 = (unsigned char) Bbuf[i + 1];
        int nearestIndex = findClosestCombined(r1, r2, g1, g2, b1, b2, codebook, color);
        get<0>(sumVecs[nearestIndex].first) += r1;
        get<1>(sumVecs[nearestIndex].first) += g1;
        get<2>(sumVecs[nearestIndex].first) += b1;
        get<0>(sumVecs[nearestIndex].second) += r2;
        get<1>(sumVecs[nearestIndex].second) += g2;
        get<2>(sumVecs[nearestIndex].second) += b2;
        numVecs[nearestIndex]++;
      }
      curDiff = 0;
      for (int i = 0; i < codebook.size(); i++) {
        if (numVecs[i] > 0) {
          int newFirst = get<0>(sumVecs[i].first) / numVecs[i];
          int newSecond = get<0>(sumVecs[i].second) / numVecs[i];
          curDiff = max(curDiff, abs(get<0>(codebook[i].first) - newFirst));
          curDiff = max(curDiff, abs(get<0>(codebook[i].second) - newSecond));
          get<0>(codebook[i].first) = newFirst;
          get<0>(codebook[i].second) = newSecond;
          newFirst = get<1>(sumVecs[i].first) / numVecs[i];
          newSecond = get<1>(sumVecs[i].second) / numVecs[i];
          curDiff = max(curDiff, abs(get<1>(codebook[i].first) - newFirst));
          curDiff = max(curDiff, abs(get<1>(codebook[i].second) - newSecond));
          get<1>(codebook[i].first) = newFirst;
          get<1>(codebook[i].second) = newSecond;
          newFirst = get<2>(sumVecs[i].first) / numVecs[i];
          newSecond = get<2>(sumVecs[i].second) / numVecs[i];
          curDiff = max(curDiff, abs(get<2>(codebook[i].first) - newFirst));
          curDiff = max(curDiff, abs(get<2>(codebook[i].second) - newSecond));
          get<2>(codebook[i].first) = newFirst;
          get<2>(codebook[i].second) = newSecond;
        }
      }
    }

    //set pixels to codebook:
    int closestIndex;
    for (int i = 0; i < width * height - 1; i += 2) {
      closestIndex = findClosestCombined((int) (unsigned char) Rbuf[i], (int) (unsigned char) Rbuf[i + 1], (int) (unsigned char) Gbuf[i], (int) (unsigned char) Gbuf[i + 1], (int) (unsigned char) Bbuf[i], (int) (unsigned char) Bbuf[i + 1], codebook, color);
      if (color) {
        newR[i] = get<0>(codebook[closestIndex].first);
        newR[i + 1] = get<0>(codebook[closestIndex].second);
        newG[i] = get<1>(codebook[closestIndex].first);
        newG[i + 1] = get<1>(codebook[closestIndex].second);
        newB[i] = get<2>(codebook[closestIndex].first);
        newB[i + 1] = get<2>(codebook[closestIndex].second);
      } else { //grayscale
        newR[i] = get<0>(codebook[closestIndex].first);
        newR[i + 1] = get<0>(codebook[closestIndex].second);
        newG[i] = newR[i];
        newB[i] = newR[i];
        newG[i + 1] = newR[i + 1];
        newB[i + 1] = newR[i + 1];
      }
    }
  } else { //m is a perfect square
    m = sqrt(m);
    //initialize codebook:
    vector<vector<vector<tuple<int, int, int>>>> codebook;
    vector<vector<vector<tuple<int, int, int>>>> sumVecs(n, vector<vector<tuple<int, int, int>>>(m, vector<tuple<int, int, int>>(m, make_tuple(0, 0, 0))));
    vector<int> numVecs(n, 0);
    for (int i = 0; i < n; i++) {
      int value = i * 256 / n;
      vector<vector<tuple<int, int, int>>> block(m, vector<tuple<int, int, int>>(m, make_tuple(value, value, value)));
      codebook.push_back(block);
    }

    //codebook refinement:
    int curDiff = 255;
    int iter = 0;
    while (curDiff > 10 && iter++ < 50) {
      vector<vector<vector<tuple<int, int, int>>>> sumVecs(n, vector<vector<tuple<int, int, int>>>(m, vector<tuple<int, int, int>>(m, make_tuple(0, 0, 0))));
      numVecs = vector<int>(n, 0);

      for (int y = 0; y <= height - m; y += m) {
        for (int x = 0; x <= width - m; x += m) { //traverse, finding each block's top left corner
          int nearestIndex = findClosestSq(Rbuf, Gbuf, Bbuf, x, y, codebook, color, width);
          for (int j = 0; j < m; j++) {
            for (int k = 0; k < m; k++) { //each pixel in the block
              get<0>(sumVecs[nearestIndex][j][k]) += (int) (unsigned char) Rbuf[(y + j) * width + (x + k)];
              get<1>(sumVecs[nearestIndex][j][k]) += (int) (unsigned char) Gbuf[(y + j) * width + (x + k)];
              get<2>(sumVecs[nearestIndex][j][k]) += (int) (unsigned char) Bbuf[(y + j) * width + (x + k)];
            }
          }
          numVecs[nearestIndex] ++; //r, g, and b
          curDiff = 0;
          for (int i = 0; i < codebook.size(); i++) { //update every entry, calc diff
            if (numVecs[i] > 0) { //avoid div by 0
              for (int j = 0; j < m; j++) {
                for (int k = 0; k < m; k++) {
                  int newValue = get<0>(sumVecs[i][j][k]) / numVecs[i];
                  curDiff = max(curDiff, abs(get<0>(codebook[i][j][k]) - newValue));
                  get<0>(codebook[i][j][k]) = newValue;
                  if (color) {
                    newValue = get<1>(sumVecs[i][j][k]) / numVecs[i];
                    curDiff = max(curDiff, abs(get<1>(codebook[i][j][k]) - newValue));
                    get<1>(codebook[i][j][k]) = newValue;
                    newValue = get<2>(sumVecs[i][j][k]) / numVecs[i];
                    curDiff = max(curDiff, abs(get<2>(codebook[i][j][k]) - newValue));
                    get<2>(codebook[i][j][k]) = newValue;
                  }
                }
              }
            }
          }
        }
      }
    }
    //calc newR
    int closestIndex;
    for (int y = 0; y <= height - m; y += m) { 
      for (int x = 0; x <= width - m; x += m) { //each block top left corner
        for (int dy = 0; dy < m; dy++) {
          for (int dx = 0; dx < m; dx++) { //for each pixel in the block
            int index = (y + dy) * width + (x + dx);
            closestIndex = findClosestSq(Rbuf, Rbuf, Rbuf, x, y, codebook, false, width);
            newR[index] = get<0>(codebook[closestIndex][dy][dx]); //red
            if (color) {
              newG[index] = get<1>(codebook[closestIndex][dy][dx]);
              newB[index] = get<2>(codebook[closestIndex][dy][dx]);
            } else {
              newG[index] = newR[index]; 
              newB[index] = newR[index];
            }
          }
        }
      }
    }
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
      if (color) {
        inData[3 * (i + j * newWidth) + 1] = Gbuf[i + j * width];
        inData[3 * (i + j * newWidth) + 2] = Bbuf[i + j * width];
      } else {
        inData[3 * (i + j * newWidth) + 1] = Rbuf[i + j * width];
        inData[3 * (i + j * newWidth) + 2] = Rbuf[i + j * width];
      }
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