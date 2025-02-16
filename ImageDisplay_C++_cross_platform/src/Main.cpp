#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>

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
  MyFrame(const wxString &title, string imagePath, float s, int q, int m);

 private:
  void OnPaint(wxPaintEvent &event);
  wxImage inImage;
  wxScrolledWindow *scrolledWindow;
  int width;
  int height;
};

/** Utility function to read image data */
unsigned char *readImageData(string imagePath, int width, int height, float scale, int quant, int mode);

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
  if (wxApp::argc != 5 && wxApp::argc != 4) {
    cerr << "The executable should be invoked with exactly one filepath "
            "argument, Scale (0.0-1.0), Quantization (1-8), and Mode (-1-255). "
            "Example ./MyImageApplication '../../Lena_512_512.rgb'"
         << endl;
    exit(1);
  }
  string imagePath = wxApp::argv[1].ToStdString();
  cout << "First argument: " << wxApp::argv[0] << endl;
  cout << "Second argument: " << wxApp::argv[1] << endl;
  cout << "Third argument: " << wxApp::argv[2] << endl;
  cout << "Fourth argument: " << wxApp::argv[3] << endl;

  MyFrame *frame;
  if (wxApp::argc == 5) { //pivot provided
    cout << "Fifth argument: " << wxApp::argv[4] << endl;
    frame = new MyFrame("Image Display", imagePath, stof((wxApp::argv[2]).ToStdString()), stoi((wxApp::argv[3]).ToStdString()), stoi((wxApp::argv[4]).ToStdString()));
  } else {
    frame = new MyFrame("Image Display", imagePath, stof((wxApp::argv[2]).ToStdString()), stoi((wxApp::argv[3]).ToStdString()), -2);
  } 
  frame->Show(true);

  // return true to continue, false to exit the application
  return true;
}

/**
 * Constructor for the MyFrame class.
 * Here we read the pixel data from the file and set up the scrollable window.
 */
MyFrame::MyFrame(const wxString &title, string imagePath, float s, int q, int m)
    : wxFrame(NULL, wxID_ANY, title) {

  // Modify the height and width values here to read and display an image with
  // different dimensions.    
  width = 512;
  height = 512;

  unsigned char *inData = readImageData(imagePath, width, height, s, q, m);
  
  width *= s;
  height *= s;

  // the last argument is static_data, if it is false, after this call the
  // pointer to the data is owned by the wxImage object, which will be
  // responsible for deleting it. So this means that you should not delete the
  // data yourself.
  inImage.SetData(inData, width, height, false);

  // Set up the scrolled window as a child of this frame
  scrolledWindow = new wxScrolledWindow(this, wxID_ANY);
  scrolledWindow->SetScrollbars(10, 10, width, height);
  scrolledWindow->SetVirtualSize(width, height);

  // Bind the paint event to the OnPaint function of the scrolled window
  scrolledWindow->Bind(wxEVT_PAINT, &MyFrame::OnPaint, this);

  // Set the frame size
  SetClientSize(width, height);

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

int logScale(int val, int pivot, float base) { 
  double distance = abs(val - pivot);
  double scaledDistance = log(1 + distance) / log(base);
  return (val < pivot ? -scaledDistance : scaledDistance); //which side of pivot?
}

int logQuant(int value, int pivot, int numIntervals, float base = 2.0) { //adjustable base for logarithmic scaling
    if (value == pivot) { //avoid log(0)
        return pivot;
    }

    //normalize, find index
    double logMin = logScale(0, pivot, base);
    double logMax = logScale(255, pivot, base);
    double intervalSize = (logMax - logMin) / numIntervals;
    int intervalIndex = min((int) ((logScale(value, pivot, base) - logMin) / intervalSize), numIntervals - 1);

    //map to middle of interval
    double quantizedLogValue = logMin + (intervalIndex + 0.5) * intervalSize;

    //apply to original range
    double distance = quantizedLogValue >= 0 ? pow(base, quantizedLogValue) - 1 : -(pow(base, -quantizedLogValue) - 1);
    int quantizedValue = pivot + distance;

    //clamp
    return max(0, min(quantizedValue, 255));
}

/** Utility function to read image data */
unsigned char *readImageData(string imagePath, int width, int height, float scale, int quant, int mode) {

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

  /* find average value to calculate pivot, if none given */
  if (mode == -2) {
    long avg = 0;
    for (int i = 0; i < width*height; i += 100) {
      avg += Rbuf[i];
      avg += Gbuf[i];
      avg += Bbuf[i];
    }
    avg /= ((width*height)/5);
    avg /= 3;
    mode = avg;
    cout << "Pivot calculated: " << mode << endl;
  }

  /* scale down */
  int newSize = (int) (scale * (float) width * scale * (float) height);
  vector<char> newR(newSize);
  vector<char> newG(newSize);
  vector<char> newB(newSize);

  float reverseScale = 1/scale;
  int currHeight;
  int currWidth;
  int round;
  int numIntervals = 1;
  for (int i = 0; i < quant; i++)
    numIntervals *= 2; //2^quant
  int bitIntervals = 256 / numIntervals; //round to the nearest
  std::set<int> values;

  for (int i = 0; i < newSize; i++) {
    currHeight = i / (int)(height * scale);
    currWidth = i % (int) (width * scale);
    int x = ((float) currWidth * reverseScale);
    int y = ((float) currHeight * reverseScale);
    if (mode == -1) { //uniform
      if (scale < 1 && x-1 >= 0 && x+1 < width && y+1 < height && y-1 >= 0) {
        round = (int)(unsigned char)Rbuf[y*width + x]/9 + (int)(unsigned char)Rbuf[y*width + x-1]/9 + (int)(unsigned char)Rbuf[y*width + x+1]/9 + (int)(unsigned char)Rbuf[y*width + x + width]/9 + (int)(unsigned char)Rbuf[y*width + x-1 + width]/9 + (int)(unsigned char)Rbuf[y*width + x+1 + width]/9 + (int)(unsigned char)Rbuf[y*width + x - width]/9 + (int)(unsigned char)Rbuf[y*width + x-1 - width]/9 + (int)(unsigned char)Rbuf[y*width + x+1 - width]/9;
        round = round / bitIntervals * bitIntervals; //round to the nearest value
        newR[i] = round + (bitIntervals / 2);
        if (values.find(round + (bitIntervals / 2)) == values.end())
          values.insert(round + (bitIntervals / 2));
        round = (int)(unsigned char)Gbuf[y*width + x]/9 + (int)(unsigned char)Gbuf[y*width + x-1]/9 + (int)(unsigned char)Gbuf[y*width + x+1]/9 + (int)(unsigned char)Gbuf[y*width + x + width]/9 + (int)(unsigned char)Gbuf[y*width + x-1 + width]/9 + (int)(unsigned char)Gbuf[y*width + x+1 + width]/9 + (int)(unsigned char)Gbuf[y*width + x - width]/9 + (int)(unsigned char)Gbuf[y*width + x-1 - width]/9 + (int)(unsigned char)Gbuf[y*width + x+1 - width]/9;
        round = round / bitIntervals * bitIntervals;
        newG[i] = round + (bitIntervals / 2);
        round = (int)(unsigned char)Bbuf[y*width + x]/9 + (int)(unsigned char)Bbuf[y*width + x-1]/9 + (int)(unsigned char)Bbuf[y*width + x+1]/9 + (int)(unsigned char)Bbuf[y*width + x + width]/9 + (int)(unsigned char)Bbuf[y*width + x-1 + width]/9 + (int)(unsigned char)Bbuf[y*width + x+1 + width]/9 + (int)(unsigned char)Bbuf[y*width + x - width]/9 + (int)(unsigned char)Bbuf[y*width + x-1 - width]/9 + (int)(unsigned char)Bbuf[y*width + x+1 - width]/9;
        round = round / bitIntervals * bitIntervals;
        newB[i] = round + (bitIntervals / 2);
      } else { //edge cases don't use filter, scale 1 doesn't use filter
        round = (int)(unsigned char)Rbuf[y*width + x] / bitIntervals * bitIntervals + (bitIntervals / 2);
        if (values.find(round) == values.end()) //for printing
          values.insert(round);
        newR[i] = round;
        newG[i] = (int)(unsigned char)Gbuf[y*width + x] / bitIntervals * bitIntervals + (bitIntervals / 2);
        newB[i] = (int)(unsigned char)Bbuf[y*width + x] / bitIntervals * bitIntervals + (bitIntervals / 2);
      }
    } else { //non-uniform
       if (scale < 1 && y*width + x-width-1 >= 0 && y*width + x+width+1 < width*height) {
        round = (int)(unsigned char)Rbuf[y*width + x]/9 + (int)(unsigned char)Rbuf[y*width + x-1]/9 + (int)(unsigned char)Rbuf[y*width + x+1]/9 + (int)(unsigned char)Rbuf[y*width + x + width]/9 + (int)(unsigned char)Rbuf[y*width + x-1 + width]/9 + (int)(unsigned char)Rbuf[y*width + x+1 + width]/9 + (int)(unsigned char)Rbuf[y*width + x - width]/9 + (int)(unsigned char)Rbuf[y*width + x-1 - width]/9 + (int)(unsigned char)Rbuf[y*width + x+1 - width]/9;
        round = logQuant(round, mode, numIntervals);
        if (values.find(round) == values.end())
          values.insert(round);
        newR[i] = round;
        round = (int)(unsigned char)Gbuf[y*width + x]/9 + (int)(unsigned char)Gbuf[y*width + x-1]/9 + (int)(unsigned char)Gbuf[y*width + x+1]/9 + (int)(unsigned char)Gbuf[y*width + x + width]/9 + (int)(unsigned char)Gbuf[y*width + x-1 + width]/9 + (int)(unsigned char)Gbuf[y*width + x+1 + width]/9 + (int)(unsigned char)Gbuf[y*width + x - width]/9 + (int)(unsigned char)Gbuf[y*width + x-1 - width]/9 + (int)(unsigned char)Gbuf[y*width + x+1 - width]/9;
        round = logQuant(round, mode, numIntervals);
        newG[i] = round;
        round = (int)(unsigned char)Bbuf[y*width + x]/9 + (int)(unsigned char)Bbuf[y*width + x-1]/9 + (int)(unsigned char)Bbuf[y*width + x+1]/9 + (int)(unsigned char)Bbuf[y*width + x + width]/9 + (int)(unsigned char)Bbuf[y*width + x-1 + width]/9 + (int)(unsigned char)Bbuf[y*width + x+1 + width]/9 + (int)(unsigned char)Bbuf[y*width + x - width]/9 + (int)(unsigned char)Bbuf[y*width + x-1 - width]/9 + (int)(unsigned char)Bbuf[y*width + x+1 - width]/9;
        round = logQuant(round, mode, numIntervals);
        newB[i] = round;
      } else { //edge cases don't use filter
        round = logQuant((int)(unsigned char)Rbuf[y*width + x], mode, numIntervals);
        if (values.find(round) == values.end())
          values.insert(round);
        newR[i] = round;
        newG[i] = logQuant((int)(unsigned char)Gbuf[y*width + x], mode, numIntervals);
        newB[i] = logQuant((int)(unsigned char)Bbuf[y*width + x], mode, numIntervals);
      }
    }
  }

  cout << "Red values found:" << endl;
  for (auto & i:values)
    cout << i << endl;

  /**
   * Allocate a buffer to store the pixel values
   * The data must be allocated with malloc(), NOT with operator new. wxWidgets
   * library requires this.
   */
  unsigned char *inData =
      (unsigned char *)malloc(newSize * 3 * sizeof(unsigned char));
      
  for (int i = 0; i < newSize; i++) {
    // We populate RGB values of each pixel in that order
    // RGB.RGB.RGB and so on for all pixels
    inData[3 * i] = newR[i];
    inData[3 * i + 1] = newG[i];
    inData[3 * i + 2] = newB[i];
  }

  return inData;
}

wxIMPLEMENT_APP(MyApp);
