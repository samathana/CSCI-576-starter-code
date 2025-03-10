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

  int n = 16;
  int sqrtN = sqrt(n);
  int intervalSize = 256/sqrtN;
  vector<pair<int, int>> codebook;
  for (int i = 0; i < sqrtN; i++) {
    for (int j = 0; j < sqrtN; j++) {
      codebook.push_back(make_pair(i * intervalSize + (intervalSize / 2), j * intervalSize + (intervalSize / 2)));
    }
  }

  int firstVal;
  int secondVal;
  for (int i = 0; i < width * height - 1; i += 2) {
    firstVal = round(static_cast<float>((unsigned char) Rbuf[i]) / intervalSize) - 1;
    secondVal = round(static_cast<float>((unsigned char) Rbuf[i + 1]) / intervalSize) - 1;
    newR[i] = codebook[firstVal * sqrtN + secondVal].first;
    newR[i + 1] = codebook[firstVal * sqrtN + secondVal].second;
    firstVal = round(static_cast<float>((unsigned char) Gbuf[i]) / intervalSize) - 1;
    secondVal = round(static_cast<float>((unsigned char) Gbuf[i + 1]) / intervalSize) - 1;
    newG[i] = codebook[firstVal * sqrtN + secondVal].first;
    newG[i + 1] = codebook[firstVal * sqrtN + secondVal].second;    
    firstVal = round(static_cast<float>((unsigned char) Bbuf[i]) / intervalSize) - 1;
    secondVal = round(static_cast<float>((unsigned char) Bbuf[i + 1]) / intervalSize) - 1;
    newB[i] = codebook[firstVal * sqrtN + secondVal].first;
    newB[i + 1] = codebook[firstVal * sqrtN + secondVal].second;
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
      // We populate RGB values of each pixel in that order
      // RGB.RGB.RGB and so on for all pixels
      inData[3 * (i + j * newWidth)] = newR[i + j * width];
      inData[3 * (i + j * newWidth) + 1] = newG[i + j * width];
      inData[3 * (i + j * newWidth) + 2] = newB[i + j * width];
    }
  }

  return inData;
}

wxIMPLEMENT_APP(MyApp);