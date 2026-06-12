#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <opencv2/face.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/opencv.hpp>
#include <thread>
#include <vector>

cv::Ptr<cv::face::LBPHFaceRecognizer> model =
    cv::face::LBPHFaceRecognizer::create();
std::chrono::steady_clock::time_point last_sample_time =
    std::chrono::steady_clock::now();

bool face_detected_this_frame = false;
std::chrono::steady_clock::time_point last_seen_time;
const int DOOR_TIMEOUT_MS = 3000;

std::chrono::steady_clock::time_point last_door_tick =
    std::chrono::steady_clock::now();

std::vector<cv::Mat> training_images;
std::vector<int> training_labels;

double minimum_confidence = 70.0;

enum AppState { TRAINING_MODE, MODEL_TRAINING, RECOGNITION_MODE };

struct FeaturesData {
  std::mutex mtx;
  std::condition_variable cv;
  bool quit = false;

  cv::Mat frame;
  bool has_new_frame = false;

  // Force each atomic to its own 64-byte cache line
  alignas(64) std::atomic<AppState> State{TRAINING_MODE};
  alignas(64) std::atomic<bool> flag_open_door{false};
  alignas(64) std::atomic<float> door_position{0.0f};
  alignas(64) std::atomic<double> last_confidence{0.0};

  std::vector<cv::Rect> features;
};

void processing_worker(FeaturesData &dataInstance,
                       cv::CascadeClassifier &face_cascade,
                       std::atomic<AppState> &State) {
  while (true) {
    cv::Mat grayscale_frame;
    {
      std::unique_lock<std::mutex> lock(dataInstance.mtx);
      dataInstance.cv.wait(lock, [&] {
        return dataInstance.has_new_frame || dataInstance.quit;
      });

      if (dataInstance.quit) {
        std::cout << "Exit." << std::endl;
        return;
      }

      grayscale_frame = dataInstance.frame.clone();
    }
    std::vector<cv::Rect> local_features;
    face_cascade.detectMultiScale(grayscale_frame, local_features, 1.1, 4, 0,
                                  cv::Size(30, 30));
    std::vector<cv::Rect> verified_features;
    if (!local_features.empty()) {
      cv::Rect face_rect = local_features[0];
      cv::Mat face_roi = grayscale_frame(face_rect);
      cv::Mat resized_face;
      cv::resize(face_roi, resized_face, cv::Size(128, 128));
      if (State.load() == TRAINING_MODE) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - last_sample_time)
                           .count();
        if (elapsed >= 500) {
          if (training_images.size() < 20) {
            training_images.push_back(resized_face.clone());
            training_labels.push_back(1);
            last_sample_time = now;
            if (training_images.size() >= 20) {
              State.store(MODEL_TRAINING);
            }
          }
        }
      } else if (State.load() == RECOGNITION_MODE) {
        int predicted_label = -1;
        double conf = 0.0;

        model->predict(resized_face, predicted_label, conf);

        if (predicted_label == 1 && conf < 70.0) {
          verified_features.push_back(face_rect);
          dataInstance.flag_open_door.store(true);
          last_seen_time = std::chrono::steady_clock::now();
        } else {
          dataInstance.flag_open_door.store(false);
        }
        dataInstance.last_confidence.store(conf);
      }
    }
    {
      std::lock_guard<std::mutex> lock(dataInstance.mtx);
      dataInstance.features = verified_features;
    }
  }
}

void drawDoorWindow(const std::string &win_name, FeaturesData &dataInstance,
                    const std::string &conf) {
  // Modern Dark Mode Palette
  const cv::Scalar COLOR_BG(18, 18, 18);            // #121212 Deep Charcoal
  const cv::Scalar COLOR_CONTAINER(30, 30, 30);     // #1E1E1E Elegant Gray
  const cv::Scalar COLOR_BORDER(45, 45, 45);        // #2D2D2D Clean Grid Line
  const cv::Scalar COLOR_PANEL(160, 110, 40);       // Modern Slate Blue
  const cv::Scalar COLOR_TEXT_MUTED(140, 140, 140); // Muted secondary gray

  cv::Mat door_canvas = cv::Mat(400, 400, CV_8UC3, COLOR_BG);

  auto current_tick = std::chrono::steady_clock::now();
  float delta_time = std::chrono::duration_cast<std::chrono::microseconds>(
                         current_tick - last_door_tick)
                         .count() /
                     1000000.0f;
  last_door_tick = current_tick;
  if (delta_time > 0.1f)
    delta_time = 0.1f;

  // Use FONT_HERSHEY_PLAIN for an ultra-sharp, non-colliding terminal/system
  // aesthetic
  const int MODERN_FONT = cv::FONT_HERSHEY_PLAIN;
  const double FONT_SIZE = 1.0;
  const int THICKNESS = 1;

  if (dataInstance.State.load() == TRAINING_MODE) {
    float progress = static_cast<float>(training_images.size()) / 20.0f;

    cv::rectangle(door_canvas, cv::Point(40, 160), cv::Point(360, 240),
                  COLOR_CONTAINER, -1);
    cv::rectangle(door_canvas, cv::Point(40, 160), cv::Point(360, 240),
                  COLOR_BORDER, 1);

    // Progress bar track
    cv::rectangle(door_canvas, cv::Point(60, 205), cv::Point(340, 212),
                  cv::Scalar(30, 30, 30), -1);
    if (progress > 0.0f) {
      int current_bar_width = static_cast<int>(progress * 280);
      cv::rectangle(door_canvas, cv::Point(60, 205),
                    cv::Point(60 + current_bar_width, 212),
                    cv::Scalar(70, 220, 70), -1);
    }

    std::string pct_text =
        "TRAINING: " + std::to_string(static_cast<int>(progress * 100)) + "%";
    cv::putText(door_canvas, pct_text, cv::Point(60, 190), MODERN_FONT,
                FONT_SIZE, cv::Scalar(240, 240, 240), THICKNESS, cv::LINE_AA);

  } else if (dataInstance.State.load() == RECOGNITION_MODE) {
    if (dataInstance.flag_open_door.load()) {
      if (std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - last_seen_time)
              .count() > DOOR_TIMEOUT_MS)
        dataInstance.flag_open_door.store(false);
    }

    float pos = dataInstance.door_position.load();
    pos = dataInstance.flag_open_door.load()
              ? std::min(1.0f, pos + 1.5f * delta_time)
              : std::max(0.0f, pos - 1.5f * delta_time);
    dataInstance.door_position.store(pos);

    int slide = static_cast<int>(pos * 145);

    // Frame Layout
    cv::rectangle(door_canvas, cv::Point(40, 70), cv::Point(360, 340),
                  COLOR_CONTAINER, -1);
    cv::rectangle(door_canvas, cv::Point(40, 70), cv::Point(360, 340),
                  COLOR_BORDER, 1);
    cv::rectangle(door_canvas, cv::Point(45, 75), cv::Point(355, 335),
                  cv::Scalar(12, 12, 12), -1);

    // Left Sliding Panel
    cv::rectangle(door_canvas, cv::Point(45 - slide, 75),
                  cv::Point(200 - slide, 335), COLOR_PANEL, -1);
    cv::rectangle(door_canvas, cv::Point(192 - slide, 75),
                  cv::Point(200 - slide, 335), cv::Scalar(200, 140, 50), -1);

    // Right Sliding Panel
    cv::rectangle(door_canvas, cv::Point(200 + slide, 75),
                  cv::Point(355 + slide, 335), COLOR_PANEL, -1);
    cv::rectangle(door_canvas, cv::Point(200 + slide, 75),
                  cv::Point(208 + slide, 335), cv::Scalar(200, 140, 50), -1);

    // Elegant Dynamic Status Pill
    cv::rectangle(door_canvas, cv::Point(120, 22), cv::Point(280, 48),
                  COLOR_CONTAINER, -1);
    cv::rectangle(door_canvas, cv::Point(120, 22), cv::Point(280, 48),
                  COLOR_BORDER, 1);

    if (pos > 0.9f) {
      cv::putText(door_canvas, "STATUS: OPEN", cv::Point(145, 39), MODERN_FONT,
                  FONT_SIZE, cv::Scalar(80, 240, 80), THICKNESS, cv::LINE_AA);
    } else if (pos < 0.1f) {
      cv::putText(door_canvas, "STATUS: LOCKED", cv::Point(137, 39),
                  MODERN_FONT, FONT_SIZE, cv::Scalar(90, 90, 250), THICKNESS,
                  cv::LINE_AA);
    } else {
      cv::putText(door_canvas, "STATUS: MOVING", cv::Point(137, 39),
                  MODERN_FONT, FONT_SIZE, cv::Scalar(80, 190, 240), THICKNESS,
                  cv::LINE_AA);
    }

    // Bottom Footer
    cv::putText(door_canvas, "CONFIDENCE: " + conf, cv::Point(40, 375),
                MODERN_FONT, FONT_SIZE, COLOR_TEXT_MUTED, THICKNESS,
                cv::LINE_AA);
  }

  cv::imshow(win_name, door_canvas);
}

int main(int argc, char *argv[]) {
  cv::CascadeClassifier face_cascade;
  if (!face_cascade.load("haarcascade_frontalface_default.xml")) {
    std::cerr << "Couldn't load cascade filter! Are you sure it's named "
                 "'haarcascade_frontalface_default.xml'?"
              << std::endl;
    return -1;
  }

  const char *const window_name{"Automatic Door Opener"};
  const char *const door_window_name{"Electronic Shield Door"};

  cv::namedWindow(window_name, cv::WINDOW_GUI_NORMAL | cv::WINDOW_AUTOSIZE);
  cv::namedWindow(door_window_name,
                  cv::WINDOW_GUI_NORMAL | cv::WINDOW_AUTOSIZE);

  cv::VideoCapture capture(0);
  if (!capture.isOpened()) {
    std::cerr << "Cannot open video capture." << std::endl;
    return -1;
  }

  FeaturesData dataInstance;

  std::thread worker(processing_worker, std::ref(dataInstance),
                     std::ref(face_cascade), std::ref(dataInstance.State));

  cv::Mat image;

  int frame_count = 0;
  while (capture.read(image) and (!image.empty())) {
    cv::Mat grayscale_image;
    cv::cvtColor(image, grayscale_image, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(grayscale_image, grayscale_image);

    frame_count++;

    if (dataInstance.State.load() == MODEL_TRAINING) {
      std::cout << "Compiling LBPH model. Please wait..." << std::endl;
      model->train(training_images, training_labels);
      std::cout << "Model Trained! Switching to Recognition Mode." << std::endl;
      dataInstance.State.store(RECOGNITION_MODE);
    }

    if (frame_count % 5 == 0) {
      {
        std::lock_guard<std::mutex> lock(dataInstance.mtx);
        dataInstance.frame = grayscale_image.clone();
        dataInstance.has_new_frame = true;
      }
      dataInstance.cv.notify_one();
    }

    std::vector<cv::Rect> features_to_draw;
    {
      std::lock_guard<std::mutex> lock(dataInstance.mtx);
      features_to_draw = dataInstance.features;
    }

    for (auto &&feature : features_to_draw) {
      cv::rectangle(image, feature, cv::Scalar(0, 255, 0), 2);
    }

    double live_conf = dataInstance.last_confidence.load();

    double short_conf = std::trunc(live_conf * 100.0) / 100.0;

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << short_conf;

    drawDoorWindow(door_window_name, dataInstance, ss.str());

    cv::imshow(window_name, image);
    int key = cv::waitKey(10);
    if (key == 'q' || key == 'Q') {
      {
        std::lock_guard<std::mutex> lock(dataInstance.mtx);
        dataInstance.quit = true;
      }
      dataInstance.cv.notify_one();
      if (worker.joinable()) {
        worker.join();
      } else {
        std::cout << "Can't join!" << std::endl;
      }
      std::_Exit(EXIT_SUCCESS);
    }
  }
  return EXIT_SUCCESS;
}
