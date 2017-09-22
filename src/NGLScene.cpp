#include <QMouseEvent>
#include <QApplication>

#include "NGLScene.h"
#include <ngl/Camera.h>
#include <ngl/Light.h>
#include <ngl/Transformation.h>
#include <ngl/Material.h>
#include <ngl/NGLInit.h>
#include <ngl/VAOPrimitives.h>
#include <ngl/ShaderLib.h>




NGLScene::NGLScene()
{
  setTitle("Frustum Culling");
  // Now set the initial GLWindow attributes to default values
  // now set the inital camera values
  m_cameraIndex=0;
  m_moveMode=CamMode::MOVEEYE;
  m_drawHelp=true;
  m_fov=65.0f;
  m_aspect=1024.0f/768.0f;
  m_near=1.0f;
  m_far=10.0f;
}

void NGLScene::createCameras()
{
  // create a load of cameras
  ngl::Camera cam;
  ngl::Camera Tcam;
  // set the different vectors for the camera positions
  ngl::Vec3 Eye(0,0,1);
  ngl::Vec3 Look(0,0,0);
  ngl::Vec3 Up(0,1,0);

  ngl::Vec3 TEye(0,50,0);
  ngl::Vec3 TLook(0.0f,0.0f,0.0f);
  ngl::Vec3 Tup(0,0,1);

  // finally set the cameras shape and position
  cam.set(Eye,Look,Up);
  cam.setShape(m_fov,m_aspect, m_near,m_far);
  m_cameras.push_back(cam);
  Tcam.set(TEye,TLook,Tup);
  Tcam.setShape(80,m_aspect, 0.5,100);
  m_cameras.push_back(Tcam);
}

NGLScene::~NGLScene()
{
  std::cout<<"Shutting down NGL, removing VAO's and Shaders\n";
}

void NGLScene::resizeGL( int _w, int _h )
{
  m_win.width  = static_cast<int>( _w * devicePixelRatio() );
  m_win.height = static_cast<int>( _h * devicePixelRatio() );
}


void NGLScene::initializeGL()
{
  // we must call this first before any other GL commands to load and link the
  // gl commands from the lib, if this is not done program will crash
  ngl::NGLInit::instance();

  glClearColor(0.4f, 0.4f, 0.4f, 1.0f);			   // Grey Background
  // enable depth testing for drawing
  glEnable(GL_DEPTH_TEST);
  // enable multisampling for smoother drawing
  glEnable(GL_MULTISAMPLE);
  createCameras();
  // create an instance of the VBO primitive
  ngl::VAOPrimitives *prim=ngl::VAOPrimitives::instance();
  // create a plane
  prim->createSphere("sphere",1.0,20);

  // now to load the shader and set the values
  // grab an instance of shader manager
  // grab an instance of shader manager
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();

  shader->createShaderProgram("Phong");

  shader->attachShader("PhongVertex",ngl::ShaderType::VERTEX);
  shader->attachShader("PhongFragment",ngl::ShaderType::FRAGMENT);
  shader->loadShaderSource("PhongVertex","shaders/PhongVertex.glsl");
  shader->loadShaderSource("PhongFragment","shaders/PhongFragment.glsl");

  shader->compileShader("PhongVertex");
  shader->compileShader("PhongFragment");
  shader->attachShaderToProgram("Phong","PhongVertex");
  shader->attachShaderToProgram("Phong","PhongFragment");

  shader->linkProgramObject("Phong");
  (*shader)["Phong"]->use();

  // now pass the modelView and projection values to the shader
  shader->setUniform("Normalize",1);

  // now set the material and light values
  ngl::Material m(ngl::STDMAT::GOLD);
  m.loadToShader("material");


  ngl::Light L1(ngl::Vec3(-1,1,0),ngl::Colour(1,1,1,1),ngl::LightModes::POINTLIGHT);
  ngl::Mat4 lt= m_cameras[m_cameraIndex].getViewMatrix();
  lt.transpose();
  L1.setTransform(lt);
  L1.enable();
  L1.loadToShader("light");

  (*shader)["nglColourShader"]->use();

  shader->setUniform("Colour",1.0f,1.0f,1.0f,1.0f);
  m_text.reset( new ngl::Text(QFont("Courier",18)));
  m_text->setColour(1,1,0);
  m_text->setScreenSize(width(),height());
}


void NGLScene::loadMatricesToShader()
{
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();
  (*shader)["Phong"]->use();
  ngl::Mat4 MV;
  ngl::Mat4 MVP;
  ngl::Mat3 normalMatrix;
  ngl::Mat4 M;
  M=m_transformStack.getMatrix();
  MV=m_cameras[m_cameraIndex].getViewMatrix()*M;
  MVP=m_cameras[m_cameraIndex].getProjectionMatrix()*MV;
  normalMatrix=MV;
  normalMatrix.inverse().transpose();
  shader->setUniform("MV",MV);
  shader->setUniform("MVP",MVP);
  shader->setUniform("normalMatrix",normalMatrix);
  shader->setUniform("M",M);
  shader->setUniform("viewerPos",m_cameras[m_cameraIndex].getEye().toVec3());

}


void NGLScene::loadMatricesToColourShader()
{
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();
  (*shader)["nglColourShader"]->use();

  ngl::Mat4 MV;
  ngl::Mat4 MVP;
  MV=m_cameras[m_cameraIndex].getViewMatrix()*m_transformStack.getMatrix();
  MVP=m_cameras[m_cameraIndex].getProjectionMatrix() *MV;
  shader->setUniform("MVP",MVP);
}

void NGLScene::paintGL()
{
  // grab an instance of the shader manager
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();
  // clear the screen and depth buffer
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   // set this in the TX stack
  // now set this value in the shader for the current Camera, we do all 3 elements as they
  // can all change per frame
  shader->use("Phong");
   // get the VBO instance and draw the built in teapot
  ngl::VAOPrimitives *prim=ngl::VAOPrimitives::instance();
  // push a transform
  float ext=150.0;
  int numSpheresDrawn=0;
  int totalSpheres=0;
  ngl::Vec3 p;

  glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
  for(float z=-ext; z<ext; z+=2)
  {
    for(float y=-ext; y<ext; y+=2)
    {
      for(float x=-ext; x<ext; x+=2)
      {
        ++totalSpheres;
        p.set(x,y,z);
        if( m_cameras[0].isSphereInFrustum(p,1.0f) !=ngl::CameraIntercept::OUTSIDE )
        {
          ++numSpheresDrawn;
          // translate into position
          m_transformStack.setPosition(x,y,z);
          // pass values to the shader
          loadMatricesToShader();
          // draw
          prim->draw("sphere");
        }
      }
    }
  }
  glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

  shader->use("nglColourShader");
  m_transformStack.reset();
  loadMatricesToColourShader();
  m_cameras[0].drawFrustum();
  if( m_cameraIndex ==1)
  {
    m_transformStack.setPosition(m_cameras[0].getEye());
    loadMatricesToColourShader();
    prim->draw("cube");
  }
  // now we are going to draw the help
  if (m_drawHelp==true)
  {
   m_text->setColour(1,1,0);
   // now render the text using the QT renderText helper function
   QString text=QString("Total Spheres %1 num drawn %2").arg(totalSpheres).arg(numSpheresDrawn);
   m_text->renderText(10,18,text);
   text=QString("near %1 far %2").arg(m_near).arg(m_far);
   m_text->renderText(10,40,text);
   text=QString("fov %1").arg(m_fov);
   m_text->renderText(10,60,text);
  }

}

//----------------------------------------------------------------------------------------------------------------------
void NGLScene::mouseMoveEvent( QMouseEvent* _event )
{
  // note the method buttons() is the button state when event was called
  // that is different from button() which is used to check which button was
  // pressed when the mousePress/Release event is generated
  if ( m_win.rotate && _event->buttons() == Qt::LeftButton )
  {
    int diffx = _event->x() - m_win.origX;
    int diffy = _event->y() - m_win.origY;
    m_win.spinXFace += static_cast<int>( 0.5f * diffy );
    m_win.spinYFace += static_cast<int>( 0.5f * diffx );
    m_win.origX = _event->x();
    m_win.origY = _event->y();
    update();
  }
  // right mouse translate code
  else if ( m_win.translate && _event->buttons() == Qt::RightButton )
  {
    int diffX      = static_cast<int>( _event->x() - m_win.origXPos );
    int diffY      = static_cast<int>( _event->y() - m_win.origYPos );
    m_win.origXPos = _event->x();
    m_win.origYPos = _event->y();
    m_modelPos.m_x += INCREMENT * diffX;
    m_modelPos.m_y -= INCREMENT * diffY;
    update();
  }
}


//----------------------------------------------------------------------------------------------------------------------
void NGLScene::mousePressEvent( QMouseEvent* _event )
{
  // that method is called when the mouse button is pressed in this case we
  // store the value where the maouse was clicked (x,y) and set the Rotate flag to true
  if ( _event->button() == Qt::LeftButton )
  {
    m_win.origX  = _event->x();
    m_win.origY  = _event->y();
    m_win.rotate = true;
  }
  // right mouse translate mode
  else if ( _event->button() == Qt::RightButton )
  {
    m_win.origXPos  = _event->x();
    m_win.origYPos  = _event->y();
    m_win.translate = true;
  }
}

//----------------------------------------------------------------------------------------------------------------------
void NGLScene::mouseReleaseEvent( QMouseEvent* _event )
{
  // that event is called when the mouse button is released
  // we then set Rotate to false
  if ( _event->button() == Qt::LeftButton )
  {
    m_win.rotate = false;
  }
  // right mouse translate mode
  if ( _event->button() == Qt::RightButton )
  {
    m_win.translate = false;
  }
}

//----------------------------------------------------------------------------------------------------------------------
void NGLScene::wheelEvent( QWheelEvent* _event )
{

  // check the diff of the wheel position (0 means no change)
  if ( _event->delta() > 0 )
  {
    m_modelPos.m_z += ZOOM;
  }
  else if ( _event->delta() < 0 )
  {
    m_modelPos.m_z -= ZOOM;
  }
  update();
}

//----------------------------------------------------------------------------------------------------------------------

void NGLScene::keyPressEvent(QKeyEvent *_event)
{
  const static float keyIncrement=0.2;
  // this method is called every time the main window recives a key event.
  // we then switch on the key value and set the camera in the GLWindow
  // find which key and do appropriate event
  switch (_event->key())
  {
  case Qt::Key_Escape : { QApplication::exit(EXIT_SUCCESS); break; }
  case Qt::Key_1 : { m_cameraIndex=0; break; }
  case Qt::Key_2 : { m_cameraIndex=1; break; }
  case Qt::Key_3 : { updateNearFar(FarNear::DECNEAR); break; }
  case Qt::Key_4 : { updateNearFar(FarNear::INCNEAR); break; }
  case Qt::Key_5 : { updateNearFar(FarNear::DECFAR); break; }
  case Qt::Key_6 : { updateNearFar(FarNear::INCFAR); break; }

  case Qt::Key_E : { m_moveMode=CamMode::MOVEEYE; break; }
  case Qt::Key_L : { m_moveMode=CamMode::MOVELOOK; break; }
  case Qt::Key_B : { m_moveMode=CamMode::MOVEBOTH; break; }
  case Qt::Key_S : { m_moveMode=CamMode::MOVESLIDE; break; }
  case Qt::Key_H : { m_drawHelp^=true; break; }
  case Qt::Key_Plus : { ++m_fov; setCameraShape(); break; }
  case Qt::Key_Minus :{ --m_fov; setCameraShape(); break; }
  case Qt::Key_R : { m_cameras[0].roll(3);  break; }
  case Qt::Key_P : { m_cameras[0].pitch(3); break; }
  case Qt::Key_Y : { m_cameras[0].yaw(3);   break; }

  case Qt::Key_Left :
  {
    switch (m_moveMode)
    {
      case CamMode::MOVEEYE :   { m_cameras[0].moveEye(keyIncrement,0,0);  break;}
      case CamMode::MOVELOOK :  { m_cameras[0].moveLook(keyIncrement,0,0); break;}
      case CamMode::MOVEBOTH :  { m_cameras[0].moveBoth(keyIncrement,0,0); break;}
      case CamMode::MOVESLIDE : { m_cameras[0].slide(keyIncrement,0,0);    break;}
    }
  break;
  } // end move left
  case Qt::Key_Right :
  {
    switch (m_moveMode)
    {
      case CamMode::MOVEEYE :   { m_cameras[0].moveEye(-keyIncrement,0,0);  break;}
      case CamMode::MOVELOOK :  { m_cameras[0].moveLook(-keyIncrement,0,0); break;}
      case CamMode::MOVEBOTH :  { m_cameras[0].moveBoth(-keyIncrement,0,0); break;}
      case CamMode::MOVESLIDE : { m_cameras[0].slide(-keyIncrement,0,0);    break;}
    }
  break;
  } // end move right
  case Qt::Key_Up :
  {
    switch (m_moveMode)
    {
      case CamMode::MOVEEYE :   { m_cameras[0].moveEye(0,keyIncrement,0);  break;}
      case CamMode::MOVELOOK :  { m_cameras[0].moveLook(0,keyIncrement,0); break;}
      case CamMode::MOVEBOTH :  { m_cameras[0].moveBoth(0,keyIncrement,0); break;}
      case CamMode::MOVESLIDE : { m_cameras[0].slide(0,keyIncrement,0);    break;}
    }
  break;
  } // end move up
  case Qt::Key_Down :
  {
    switch (m_moveMode)
    {
      case CamMode::MOVEEYE :   { m_cameras[0].moveEye(0,-keyIncrement,0);  break;}
      case CamMode::MOVELOOK :  { m_cameras[0].moveLook(0,-keyIncrement,0); break;}
      case CamMode::MOVEBOTH :  { m_cameras[0].moveBoth(0,-keyIncrement,0); break;}
      case CamMode::MOVESLIDE : { m_cameras[0].slide(0,-keyIncrement,0);    break;}
    }
  break;
  } // end move down
  case Qt::Key_O:
  {
    switch (m_moveMode)
    {
      case CamMode::MOVEEYE :   { m_cameras[0].moveEye(0,0,keyIncrement);  break;}
      case CamMode::MOVELOOK :  { m_cameras[0].moveLook(0,0,keyIncrement); break;}
      case CamMode::MOVEBOTH :  { m_cameras[0].moveBoth(0,0,keyIncrement); break;}
      case CamMode::MOVESLIDE : { m_cameras[0].slide(0,0,keyIncrement);    break;}
    }
  break;
  } // end move out
  case Qt::Key_I :
  {
    switch (m_moveMode)
    {
      case CamMode::MOVEEYE :   { m_cameras[0].moveEye(0,0,-keyIncrement);  break;}
      case CamMode::MOVELOOK :  { m_cameras[0].moveLook(0,0,-keyIncrement); break;}
      case CamMode::MOVEBOTH :  { m_cameras[0].moveBoth(0,0,-keyIncrement); break;}
      case CamMode::MOVESLIDE : { m_cameras[0].slide(0,0,-keyIncrement);    break;}
    }
  break;
  } // end move in
  default : break;
  }
  // finally update the GLWindow and re-draw
  //if (isExposed())
    update();
}

void NGLScene::setCameraShape()
{
  m_cameras[0].setShape(m_fov,m_aspect,m_near,m_far);

  m_cameras[0].calculateFrustum();
  update();

}

void NGLScene::updateNearFar(FarNear _which)
{
	if(_which ==FarNear::DECNEAR)
	{

		if(--m_near==0.0)
		{
			m_near=1.0;
		}
	}
	else if(_which == FarNear::INCNEAR)
	{

		if(++m_near > m_far)
		{
			m_near=m_far-1;
		}
	}
	else if(_which == FarNear::DECFAR)
	{
		if(--m_far < m_near)
		{
			m_far=m_near+1;
		}
	}
	else if(_which == FarNear::INCFAR)
	{
		++m_far;
	}
setCameraShape();
}

