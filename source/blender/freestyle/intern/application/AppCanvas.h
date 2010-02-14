#ifndef ARTCANVAS_H
#define ARTCANVAS_H

#include "../stroke/Canvas.h"
#include "AppView.h"

class AppCanvas : public Canvas
{
	
public:
  AppCanvas();
  AppCanvas(AppView *iViewer);
  AppCanvas(const AppCanvas& iBrother);
  virtual ~AppCanvas();

  /*! operations that need to be done before a draw */
  virtual void preDraw();
  
  /*! operations that need to be done after a draw */
  virtual void postDraw();
  
  /*! Erases the layers and clears the canvas */
  virtual void Erase(); 
  
  /* init the canvas */
  virtual void init();
  
  /*! Reads a pixel area from the canvas */
  virtual void readColorPixels(int x,int y,int w, int h, RGBImage& oImage) const;
  /*! Reads a depth pixel area from the canvas */
  virtual void readDepthPixels(int x,int y,int w, int h, GrayImage& oImage) const;

  virtual BBox<Vec3r> scene3DBBox() const ;

	// abstract
	virtual void RenderStroke(Stroke*);
	virtual void update();


  /*! accessors */
  virtual int width() const ;
  virtual int height() const ;

	AppView *_pViewer;
  inline const AppView * viewer() const {return _pViewer;}

  /*! modifiers */
  void setViewer(AppView *iViewer) ;

  // soc
  void setPassDiffuse(float *p) {_pass_diffuse = p;}
  void setPassZ(float *p) {_pass_z = p;}
private:
  float *_pass_diffuse;
  float *_pass_z;
};


#endif
