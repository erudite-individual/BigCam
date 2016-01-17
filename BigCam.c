/****************************************************************************
    Copyright © 2015 Mikhailov Nikita Sergeyevich   
 
    This file is part of BigCam.

    BigCam is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    BigCam is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with BigCam.  If not, see <http://www.gnu.org/licenses/>.

  (Этот файл — часть BigCam.

   BigCam - свободная программа: вы можете перераспространять ее и/или
   изменять ее на условиях Стандартной общественной лицензии GNU в том виде,
   в каком она была опубликована Фондом свободного программного обеспечения;
   либо версии 3 лицензии, либо (по вашему выбору) любой более поздней
   версии.

   BigCam распространяется в надежде, что она будет полезной,
   но БЕЗО ВСЯКИХ ГАРАНТИЙ; даже без неявной гарантии ТОВАРНОГО ВИДА
   или ПРИГОДНОСТИ ДЛЯ ОПРЕДЕЛЕННЫХ ЦЕЛЕЙ. Подробнее см. в Стандартной
   общественной лицензии GNU.

   Вы должны были получить копию Стандартной общественной лицензии GNU
   вместе с этой программой. Если это не так, см.
   <http://www.gnu.org/licenses/>.)
****************************************************************************/


//---------------------------------------------------------------------------------------------------------------

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <string.h>
#include <time.h>

//---------------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------------

const int millisec = 1000;

//---------------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------------

CvVideoWriter *outputVideo;
IplImage *cvNow, *cvPrevious;
CvCapture *camera;

GtkWidget *pathChooser, *hourSpin, *minSpin, *secSpin, *hourLenSpin, *minLenSpin, *secLenSpin, *playCheckButton, *playChooser;
GtkWidget *emailOnCheck, *emailEntry, *passwordEntry, *smtpEntry, *portSpin, *hourPeriodSpin, *minPeriodSpin, *secPeriodSpin;

gboolean monitor = FALSE, first = TRUE;
long int count, length, lengthPeriod;
int num;
FILE *log_file;
char time_string[20];
struct tm *time_struct;
time_t time_seconds;


//---------------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------------

int calculateHash(IplImage *image);
int HammingDistance(int x, int y);

static gboolean videoSwitch(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
void destroy(GtkWidget *widget, CvCapture *capture);
void rewriteConfig(GtkWidget *widget, gpointer data);

char *returnConfigPath(void);
char *returnPlayConfigPath(void);
char *returnCameraConfigPath(void);

//---------------------------------------------------------------------------------------------------------------


int main()
{
  gtk_init(NULL, NULL);

  FILE *tempFile1 = fopen(returnConfigPath(), "r");
  gboolean tempBool1 = TRUE;

  if(tempFile1 == NULL || getc(tempFile1) == EOF)
    tempBool1 = FALSE;

  if(tempBool1 == FALSE)
  {
    CvFileStorage *configFS = cvOpenFileStorage(returnConfigPath(), NULL, CV_STORAGE_WRITE, NULL);
    char *home = getenv("HOME");
    cvWriteString(configFS, "path", home, 0);
    cvWriteInt(configFS, "hour", 0);
    cvWriteInt(configFS, "min", 0);
    cvWriteInt(configFS, "sec", 1);
    cvWriteInt(configFS, "hourLen", 0);
    cvWriteInt(configFS, "minLen", 0);
    cvWriteInt(configFS, "secLen", 1);
    cvWriteInt(configFS, "playOn", 0);
    cvWriteString(configFS, "playPath", "", 0);
    cvWriteInt(configFS, "emailOn", 0);
    cvWriteString(configFS, "email", "", 0);
    cvWriteString(configFS, "password", "", 0);
    cvWriteString(configFS, "smtp", "", 0);
    cvWriteInt(configFS, "port", 0);
    cvWriteInt(configFS, "hourPeriod", 0);
    cvWriteInt(configFS, "minPeriod", 0);
    cvWriteInt(configFS, "secPeriod", 0);
    cvReleaseFileStorage(&configFS);
  }

  fclose(tempFile1);

  FILE *tempFile2 = fopen(returnCameraConfigPath(), "r");
  gboolean tempBool2 = TRUE;

  if(tempFile2 == NULL || getc(tempFile2) == EOF)
    tempBool2 = FALSE;

  if(tempBool2 == FALSE)
  {
    CvFileStorage *cameraConfigFS = cvOpenFileStorage(returnCameraConfigPath(), NULL, CV_STORAGE_WRITE, NULL);
    cvWriteInt(cameraConfigFS, "camera", 0);
    cvReleaseFileStorage(&cameraConfigFS);
  }

  fclose(tempFile2);


  GtkBuilder *builder = gtk_builder_new();
  gtk_builder_add_from_file(builder, "/opt/BigCam/BigCam.glade", NULL);


  CvFileStorage *config = cvOpenFileStorage(returnConfigPath(), NULL, CV_STORAGE_READ, NULL);

  GtkWidget *window, *video, *logo, *videoEventBox, *rewriteConfigButton;
  window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
  video = GTK_WIDGET(gtk_builder_get_object(builder, "video"));
  logo = GTK_WIDGET(gtk_builder_get_object(builder, "logo"));
  videoEventBox = GTK_WIDGET(gtk_builder_get_object(builder, "videoEventBox"));
  rewriteConfigButton = GTK_WIDGET(gtk_builder_get_object(builder, "rewriteConfigButton"));

  pathChooser = GTK_WIDGET(gtk_builder_get_object(builder, "pathChooser"));
  gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(pathChooser), cvReadStringByName(config, NULL, "path", NULL));

  time(&time_seconds);
  time_struct = localtime(&time_seconds);
  strftime(time_string, 20, "%d.%m.%Y %T", time_struct);
  int time_filelen = strlen(cvReadStringByName(config, NULL, "path", NULL)) + 24;
  char *time_filename = malloc(sizeof(*time_filename) * time_filelen);
  strncpy(time_filename, cvReadStringByName(config, NULL, "path", NULL), time_filelen);
  strncat(time_filename, "/", time_filelen);
  strncat(time_filename, time_string, time_filelen);
  strncat(time_filename, ".txt", time_filelen);
  log_file = fopen(time_filename, "a+w");
  fprintf(log_file, "[%s]: приложение запущено\n", time_string);

  GtkObject *hourAdj, *minAdj, *secAdj;
  hourAdj = gtk_adjustment_new(0, 0, 99, 1, 0, 0);
  minAdj = gtk_adjustment_new(0, 0, 59, 1, 0, 0);
  secAdj = gtk_adjustment_new(0, 0, 59, 1, 0, 0);
  hourSpin = GTK_WIDGET(gtk_builder_get_object(builder, "hourSpin"));
  minSpin = GTK_WIDGET(gtk_builder_get_object(builder, "minSpin"));
  secSpin = GTK_WIDGET(gtk_builder_get_object(builder, "secSpin"));
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(hourSpin), GTK_ADJUSTMENT(hourAdj));
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(minSpin), GTK_ADJUSTMENT(minAdj));
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(secSpin), GTK_ADJUSTMENT(secAdj));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(hourSpin), cvReadIntByName(config, NULL, "hour", 0));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(minSpin), cvReadIntByName(config, NULL, "min", 0));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(secSpin), cvReadIntByName(config, NULL, "sec", 0));

  GtkObject *hourLenAdj, *minLenAdj, *secLenAdj;
  hourLenAdj = gtk_adjustment_new(0, 0, 99, 1, 0, 0);
  minLenAdj = gtk_adjustment_new(0, 0, 59, 1, 0, 0);
  secLenAdj = gtk_adjustment_new(0, 0, 59, 1, 0, 0);
  hourLenSpin = GTK_WIDGET(gtk_builder_get_object(builder, "hourLenSpin"));
  minLenSpin = GTK_WIDGET(gtk_builder_get_object(builder, "minLenSpin"));
  secLenSpin = GTK_WIDGET(gtk_builder_get_object(builder, "secLenSpin"));
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(hourLenSpin), GTK_ADJUSTMENT(hourLenAdj));
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(minLenSpin), GTK_ADJUSTMENT(minLenAdj));
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(secLenSpin), GTK_ADJUSTMENT(secLenAdj));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(hourLenSpin), cvReadIntByName(config, NULL, "hourLen", 0));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(minLenSpin), cvReadIntByName(config, NULL, "minLen", 0));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(secLenSpin), cvReadIntByName(config, NULL, "secLen", 0));

  playCheckButton = GTK_WIDGET(gtk_builder_get_object(builder, "playCheckButton"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playCheckButton), cvReadIntByName(config, NULL, "playOn", 0));
  playChooser = GTK_WIDGET(gtk_builder_get_object(builder, "playChooser"));
  gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(playChooser), cvReadStringByName(config, NULL, "playPath", NULL));
  GtkFileFilter *playFilter;
  playFilter = gtk_file_filter_new();
  gtk_file_filter_add_pattern(playFilter, "*.wav");
  gtk_file_filter_set_name(playFilter, "*.wav");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(playChooser), playFilter);

  GtkObject *portAdj;
  portAdj = gtk_adjustment_new(0, 0, 999, 1, 0, 0);
  emailOnCheck = GTK_WIDGET(gtk_builder_get_object(builder, "emailOnCheck"));
  emailEntry = GTK_WIDGET(gtk_builder_get_object(builder, "emailEntry"));
  passwordEntry = GTK_WIDGET(gtk_builder_get_object(builder, "passwordEntry"));
  smtpEntry = GTK_WIDGET(gtk_builder_get_object(builder, "smtpEntry"));
  portSpin = GTK_WIDGET(gtk_builder_get_object(builder, "portSpin"));
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(portSpin), GTK_ADJUSTMENT(portAdj));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(emailOnCheck), cvReadIntByName(config, NULL, "emailOn", 0));
  gtk_entry_set_text(GTK_ENTRY(emailEntry), cvReadStringByName(config, NULL, "email", NULL));
  gtk_entry_set_text(GTK_ENTRY(passwordEntry), cvReadStringByName(config, NULL, "password", NULL));
  gtk_entry_set_text(GTK_ENTRY(smtpEntry), cvReadStringByName(config, NULL, "smtp", NULL));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(portSpin), cvReadIntByName(config, NULL, "port", 0));

  GtkObject *hourPeriodAdj, *minPeriodAdj, *secPeriodAdj;
  hourPeriodAdj = gtk_adjustment_new(0, 0, 99, 1, 0, 0);
  minPeriodAdj = gtk_adjustment_new(0, 0, 59, 1, 0, 0);
  secPeriodAdj = gtk_adjustment_new(0, 0, 59, 1, 0, 0);
  hourPeriodSpin = GTK_WIDGET(gtk_builder_get_object(builder, "hourPeriodSpin"));
  minPeriodSpin = GTK_WIDGET(gtk_builder_get_object(builder, "minPeriodSpin"));
  secPeriodSpin = GTK_WIDGET(gtk_builder_get_object(builder, "secPeriodSpin"));
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(hourPeriodSpin), GTK_ADJUSTMENT(hourPeriodAdj));
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(minPeriodSpin), GTK_ADJUSTMENT(minPeriodAdj));
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(secPeriodSpin), GTK_ADJUSTMENT(secPeriodAdj));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(hourPeriodSpin), cvReadIntByName(config, NULL, "hourPeriod", 0));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(minPeriodSpin), cvReadIntByName(config, NULL, "minPeriod", 0));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(secPeriodSpin), cvReadIntByName(config, NULL, "secPeriod", 0));

  cvReleaseFileStorage(&config);

  CvFileStorage *cameraConfig = cvOpenFileStorage(returnCameraConfigPath(), NULL, CV_STORAGE_READ, NULL);
  num = cvReadIntByName(cameraConfig, NULL, "camera", 0);
  camera = cvCreateCameraCapture(num);
  cvReleaseFileStorage(&cameraConfig);

  gtk_builder_connect_signals(builder, NULL);
  g_signal_connect(G_OBJECT(window), "destroy", (GCallback)destroy, camera);
  g_signal_connect(G_OBJECT(videoEventBox), "button_press_event", (GCallback)videoSwitch, NULL);
  g_signal_connect(G_OBJECT(rewriteConfigButton), "clicked", (GCallback)rewriteConfig, NULL);

  g_object_unref(G_OBJECT(builder));

  GdkPixbuf *buffer = gdk_pixbuf_new_from_file("/opt/BigCam/icons/80x80.png", NULL);
  gtk_image_set_from_pixbuf(GTK_IMAGE(logo), buffer);
  buffer = gdk_pixbuf_new_from_file("/opt/BigCam/icons/16x16.png", NULL);
  gtk_window_set_icon(GTK_WINDOW(window), buffer);

  buffer = gdk_pixbuf_new_from_file("/opt/BigCam/images/not_detected.png", NULL);

  gtk_widget_show(window);

  CvFileStorage *playConfig = cvOpenFileStorage(returnPlayConfigPath(), NULL, CV_STORAGE_WRITE, NULL);
  cvWriteInt(playConfig, "play", 0);
  cvReleaseFileStorage(&playConfig);

  int now, previous;

  if(camera)
  {
    cvPrevious = cvQueryFrame(camera);
    previous = calculateHash(cvPrevious);
  }
  else
    gtk_image_set_from_pixbuf(GTK_IMAGE(video), buffer);

  while(1)
  {
    const int fps = 30;

    if(camera)
    {
      cvNow = cvQueryFrame(camera);
      now = calculateHash(cvNow);

      if(count >= 0 && monitor)
      {
        CvFont countFont;
        cvInitFont(&countFont, CV_FONT_HERSHEY_COMPLEX, 3.5, 3.5, 0, 6, CV_AA);
        char countStr[7];
        sprintf(countStr, "%ld", count / millisec + 1);
        cvPutText(cvNow, countStr, cvPoint(15, cvNow->height - 20), &countFont, CV_RGB(256, 256, 256));
      }
      else
      {
        if(HammingDistance(now, previous) > 0 && monitor)
        {
          time(&time_seconds);
          time_struct = localtime(&time_seconds);
          strftime(time_string, 20, "%d.%m.%Y %T", time_struct);
          fprintf(log_file, "[%s]: замечено движение\n", time_string);
          CvFileStorage *emailConfig = cvOpenFileStorage(returnConfigPath(), NULL, CV_STORAGE_READ, NULL);
          int emailOn = cvReadIntByName(emailConfig, NULL, "emailOn", 0);
          const char *email = cvReadStringByName(emailConfig, NULL, "email", NULL);
          const char *password = cvReadStringByName(emailConfig, NULL, "password", NULL);
          const char *smtp = cvReadStringByName(emailConfig, NULL, "smtp", NULL);
          int port = cvReadIntByName(emailConfig, NULL, "port", 0);
          cvReleaseFileStorage(&emailConfig);
          char emailCmd[97 + 3 * strlen(email) + strlen(smtp) + strlen(password)];
          sprintf(emailCmd, "sendemail -f %s -u \"BigCam\" -m \"Motion was detected!\" -t %s -s %s:%d -xu %s -xp %s -a image.jpg", email, email, smtp, port, email, password);
          
          if(lengthPeriod < 0)
          {
            if(emailOn == 1)
            {
              pid_t emailPid;
              emailPid = fork();
              time(&time_seconds);
              time_struct = localtime(&time_seconds);
              strftime(time_string, 20, "%d.%m.%Y %T", time_struct);
              fprintf(log_file, "[%s]: отправлено уведомление на электронную почту\n", time_string);
              if(emailPid == 0)
              {
                cvSaveImage("image.jpg", cvNow, 0);
                system(emailCmd);
                remove("image.jpg");
                return EXIT_SUCCESS;
              }
            }
            CvFileStorage *periodConfig = cvOpenFileStorage(returnConfigPath(), NULL, CV_STORAGE_READ, NULL);
            int hourPeriod = cvReadIntByName(periodConfig, NULL, "hourPeriod", 0);
            int minPeriod = cvReadIntByName(periodConfig, NULL, "minPeriod", 0);
            int secPeriod = cvReadIntByName(periodConfig, NULL, "secPeriod", 0);
            long int secondsPeriod = hourPeriod * 3600 + minPeriod * 60 + secPeriod;
            lengthPeriod = secondsPeriod * millisec;
            cvReleaseFileStorage(&periodConfig);
          }

          CvFileStorage *playConfig = cvOpenFileStorage(returnPlayConfigPath(), NULL, CV_STORAGE_READ, NULL);
          int play = cvReadIntByName(playConfig, NULL, "play", 0);
          cvReleaseFileStorage(&playConfig);

          CvFileStorage *otherConfig = cvOpenFileStorage(returnConfigPath(), NULL, CV_STORAGE_READ, NULL);
          int playToggle = cvReadIntByName(otherConfig, NULL, "playOn", 0);
          const char *playChar = cvReadStringByName(otherConfig, NULL, "playPath", NULL);
          cvReleaseFileStorage(&otherConfig);

          if(play == 0 && playToggle == 1)
          {
            pid_t pid;
            pid = fork();
            time(&time_seconds);
            time_struct = localtime(&time_seconds);
            strftime(time_string, 20, "%d.%m.%Y %T", time_struct);
            fprintf(log_file, "[%s]: включено звуковое оповещение\n", time_string);
            if(pid == 0)
            {
              char *cmd = malloc(sizeof(*cmd) * 6 + strlen(playChar));
              strcpy(cmd, "play ");
              strcat(cmd, playChar);
              CvFileStorage *tempConfig1 = cvOpenFileStorage(returnPlayConfigPath(), NULL, CV_STORAGE_WRITE, NULL);
              cvWriteInt(tempConfig1, "play", 1);
              cvReleaseFileStorage(&tempConfig1);
              system(cmd);
              free(cmd);
              CvFileStorage *tempConfig2 = cvOpenFileStorage(returnPlayConfigPath(), NULL, CV_STORAGE_WRITE, NULL);
              cvWriteInt(tempConfig2, "play", 0);
              cvReleaseFileStorage(&tempConfig2);
              return EXIT_SUCCESS;
            }
          }

          char currentString[20];
          struct tm *currentStruct;
          time_t currentSeconds;

          length -= fps;

          if(length < 0)
          { 
            first = !(first);
            CvFileStorage *config = cvOpenFileStorage(returnConfigPath(), NULL, CV_STORAGE_READ, NULL);
            int hourLen = cvReadIntByName(config, NULL, "hourLen", 0);
            int minLen = cvReadIntByName(config, NULL, "minLen", 0);
            int secLen = cvReadIntByName(config, NULL, "secLen", 0);
            long int secondsLen = hourLen * 3600 + minLen * 60 + secLen;
            length = secondsLen * millisec;
            cvReleaseFileStorage(&config);
          }

          if(first)
          {
            time(&time_seconds);
            time_struct = localtime(&time_seconds);
            strftime(time_string, 20, "%d.%m.%Y %T", time_struct);
            fprintf(log_file, "[%s]: начата запись в новый файл\n", time_string);

            time(&currentSeconds);
            currentStruct = localtime(&currentSeconds);
            strftime(currentString, 20, "%d.%m.%Y %T", currentStruct);

            CvFileStorage *config = cvOpenFileStorage(returnConfigPath(), NULL, CV_STORAGE_READ, NULL);
            long filenameLength = strlen(cvReadStringByName(config, NULL, "path", NULL)) + 24;
            char *filename = malloc(sizeof(*filename) * filenameLength);

            strncpy(filename, cvReadStringByName(config, NULL, "path", NULL), filenameLength);
            strncat(filename, "/", filenameLength);
            strncat(filename, currentString, filenameLength);
            strncat(filename, ".avi", filenameLength);

            outputVideo = cvCreateVideoWriter(filename, CV_FOURCC('X','V','I','D'), 5, cvSize(640, 480), 1);
            
            free(filename);
            cvReleaseFileStorage(&config);
            first = !(first);
          }

          time(&currentSeconds);
          currentStruct = localtime(&currentSeconds);
          strftime(currentString, 20, "%d.%m.%Y %T", currentStruct);

          IplImage *outputImage = cvCloneImage(cvNow);
          CvFont outputTime;
          cvInitFont(&outputTime, CV_FONT_HERSHEY_COMPLEX, 1.0, 1.0, 0, 1, CV_AA);
          cvPutText(outputImage, currentString, cvPoint(15, 30), &outputTime, CV_RGB(256, 256, 256));

          cvWriteFrame(outputVideo, outputImage);
          cvReleaseImage(&outputImage);
        }
      }

      CvFont statusFont;
      cvInitFont(&statusFont, CV_FONT_HERSHEY_COMPLEX, 0.75, 0.75, 0, 2, CV_AA);

      if(monitor)
        cvPutText(cvNow, "ON", cvPoint(15, 30), &statusFont, CV_RGB(0, 256, 0));
      else
        cvPutText(cvNow, "OFF", cvPoint(15, 30), &statusFont, CV_RGB(256, 0, 0));

      cvCvtColor(cvNow, cvNow, CV_BGR2RGB);
      GdkPixbuf *cvBuffer = gdk_pixbuf_new_from_data((guchar*) cvNow->imageData, 
        GDK_COLORSPACE_RGB, 
        FALSE, 
        cvNow->depth, 
        cvNow->width, 
        cvNow->height, 
        cvNow->widthStep, 
        NULL, NULL);
      gtk_image_set_from_pixbuf(GTK_IMAGE(video), cvBuffer);

      cvPrevious = cvNow;
      previous = now;
    }
    else
      gtk_image_set_from_pixbuf(GTK_IMAGE(video), buffer);

    char c = cvWaitKey(fps);
    count -= fps;
    lengthPeriod -= fps;
  }

  gtk_main();

  return 0;
}


//---------------------------------------------------------------------------------------------------------------

int calculateHash(IplImage *image)
{
  IplImage *src, *bin;
  src = cvCreateImage(cvSize(8, 8), image->depth, image->nChannels);
  bin = cvCreateImage(cvSize(8, 8), IPL_DEPTH_8U, 1);

  cvResize(image, src, CV_INTER_LINEAR);
  cvCvtColor(src, bin, CV_BGR2GRAY);
  CvScalar average = cvAvg(bin, NULL);
  cvThreshold(bin, bin, average.val[0], 255, CV_THRESH_BINARY);

  int hash = 0, y, x, i = 0;
  for(y = 0; y < bin->height; y++)
  {
    uchar *ptr = (uchar*) (bin->imageData + y * bin->widthStep);
    for(x = 0; x < bin->width; x++)
      if(ptr[x])
        hash |= 1 << i;
    i++;
  }

  cvReleaseImage(&src);
  cvReleaseImage(&bin);

  return hash;
}

int HammingDistance(int x, int y)
{
  int hamming = 0, val = x ^ y;

  while(val)
  {
    ++hamming;
    val &= val - 1;
  }

  return hamming;
}

//---------------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------------

static gboolean videoSwitch(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  if(event->button == 3)
  {
    cvReleaseCapture(&camera);
    CvCapture *capture;
    int i = 0;
    while(1)
    {
      capture = cvCreateCameraCapture(i);
      if(capture == NULL)
      {
        break;
        cvReleaseCapture(&capture);
      }
      else
      {
        i++;
        cvReleaseCapture(&capture);
      }
    }
    camera = cvCreateCameraCapture((num + 1) % i);
    num = (num + 1) % i;
    CvFileStorage *cameraConfig = cvOpenFileStorage(returnCameraConfigPath(), NULL, CV_STORAGE_WRITE, NULL);
    cvWriteInt(cameraConfig, "camera", num);
    cvReleaseFileStorage(&cameraConfig);
    time(&time_seconds);
    time_struct = localtime(&time_seconds);
    strftime(time_string, 20, "%d.%m.%Y %T", time_struct);
    fprintf(log_file, "[%s]: используется камера №%d\n", time_string, num);
  }
  else
  {
    monitor = !(monitor);
    if(monitor == FALSE)
    {
      first = TRUE;
      time(&time_seconds);
      time_struct = localtime(&time_seconds);
      strftime(time_string, 20, "%d.%m.%Y %T", time_struct);
      fprintf(log_file, "[%s]: детектор движения выключен\n", time_string);
    }
    else
    {
      CvFileStorage *config = cvOpenFileStorage(returnConfigPath(), NULL, CV_STORAGE_READ, NULL);
      int hour = cvReadIntByName(config, NULL, "hour", 0);
      int min = cvReadIntByName(config, NULL, "min", 0);
      int sec = cvReadIntByName(config, NULL, "sec", 0);
      long int seconds = hour * 3600 + min * 60 + sec;
      count = seconds * millisec;
      int hourLen = cvReadIntByName(config, NULL, "hourLen", 0);
      int minLen = cvReadIntByName(config, NULL, "minLen", 0);
      int secLen = cvReadIntByName(config, NULL, "secLen", 0);
      long int secondsLen = hourLen * 3600 + minLen * 60 + secLen;
      length = secondsLen * millisec;
      int hourPeriod = cvReadIntByName(config, NULL, "hourPeriod", 0);
      int minPeriod = cvReadIntByName(config, NULL, "minPeriod", 0);
      int secPeriod = cvReadIntByName(config, NULL, "secPeriod", 0);
      long int secondsPeriod = hourPeriod * 3600 + minPeriod * 60 + secPeriod;
      lengthPeriod = secondsPeriod * millisec;
      cvReleaseFileStorage(&config);
      time(&time_seconds);
      time_struct = localtime(&time_seconds);
      strftime(time_string, 20, "%d.%m.%Y %T", time_struct);
      fprintf(log_file, "[%s]: детектор движения включен\n", time_string);
    }
  }
  return TRUE;
}

void destroy(GtkWidget *widget, CvCapture *capture)
{
  gtk_main_quit();
  cvReleaseCapture(&capture);
  cvReleaseVideoWriter(&outputVideo);
  cvReleaseImage(&cvNow);
  cvReleaseImage(&cvPrevious);
  time(&time_seconds);
  time_struct = localtime(&time_seconds);
  strftime(time_string, 20, "%d.%m.%Y %T", time_struct);
  fprintf(log_file, "[%s]: приложение закрыто\n", time_string);
  fclose(log_file);
}

void rewriteConfig(GtkWidget *widget, gpointer data)
{
  char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pathChooser));
  char *playPath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(playChooser));
  const char *email = gtk_entry_get_text(GTK_ENTRY(emailEntry));
  const char *password = gtk_entry_get_text(GTK_ENTRY(passwordEntry));
  const char *smtp = gtk_entry_get_text(GTK_ENTRY(smtpEntry));
  CvFileStorage *config = cvOpenFileStorage(returnConfigPath(), NULL, CV_STORAGE_WRITE, NULL);
  
  cvWriteString(config, "path", path, 0);
  cvWriteInt(config, "hour", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(hourSpin)));
  cvWriteInt(config, "min", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(minSpin)));
  cvWriteInt(config, "sec", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(secSpin)));
  cvWriteInt(config, "hourLen", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(hourLenSpin)));
  cvWriteInt(config, "minLen", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(minLenSpin)));
  cvWriteInt(config, "secLen", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(secLenSpin)));
  cvWriteInt(config, "playOn", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(playCheckButton)));
  cvWriteString(config, "playPath", playPath, 0);
  cvWriteInt(config, "emailOn", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(emailOnCheck)));
  cvWriteString(config, "email", email, 0);
  cvWriteString(config, "password", password, 0);
  cvWriteString(config, "smtp", smtp, 0);
  cvWriteInt(config, "port", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(portSpin)));
  cvWriteInt(config, "hourPeriod", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(hourPeriodSpin)));
  cvWriteInt(config, "minPeriod", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(minPeriodSpin)));
  cvWriteInt(config, "secPeriod", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(secPeriodSpin)));
  cvReleaseFileStorage(&config);
  time(&time_seconds);
  time_struct = localtime(&time_seconds);
  strftime(time_string, 20, "%d.%m.%Y %T", time_struct);
  fprintf(log_file, "[%s]: файл настроек перезаписан\n", time_string);
}

//---------------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------------

char *returnConfigPath(void)
{
  char *home = getenv("HOME");
  char *configPath;
  configPath = malloc(sizeof(*configPath) * (strlen(getenv("HOME")) + 27));
  strcpy(configPath, home);
  strcat(configPath, "/.config/BigCam/bigcam.xml");
  return configPath;
}

char *returnPlayConfigPath(void)
{
  char *home = getenv("HOME");
  char *configPath;
  configPath = malloc(sizeof(*configPath) * (strlen(getenv("HOME")) + 33));
  strcpy(configPath, home);
  strcat(configPath, "/.config/BigCam/play-bigcam.xml");
  return configPath;
}

char *returnCameraConfigPath(void)
{
  char *home = getenv("HOME");
  char *configPath;
  configPath = malloc(sizeof(*configPath) * (strlen(getenv("HOME")) + 33));
  strcpy(configPath, home);
  strcat(configPath, "/.config/BigCam/camera-bigcam.xml");
  return configPath;
}

//---------------------------------------------------------------------------------------------------------------