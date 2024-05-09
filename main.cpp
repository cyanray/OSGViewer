#include <osgQOpenGL/osgQOpenGLWidget>

#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osg/CoordinateSystemNode>

#include <osg/Types>
#include <osgText/Text>
#include <osg/LineWidth>
#include <osg/ShapeDrawable>

#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/DriveManipulator>
#include <osgGA/KeySwitchMatrixManipulator>
#include <osgGA/StateSetManipulator>
#include <osgGA/AnimationPathManipulator>
#include <osgGA/TerrainManipulator>
#include <osgGA/SphericalManipulator>
#include <osgGA/OrbitManipulator>

#include <QApplication>
#include <QSurfaceFormat>

#include <iostream>

#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>


const auto COLOR_WHITE = osg::Vec4(1, 1, 1, 1);
const auto COLOR_RED = osg::Vec4(1, 0, 0, 1);
const auto COLOR_YELLOW = osg::Vec4(1, 1, 0, 1);


class MySegment : public osg::Group
{
public:
    enum FeaturePoint
    {
        START_POINT,
        END_POINT,
        CENTER_POINT
    };


    MySegment(const osg::Vec3& start, const osg::Vec3& end): m_StartPoint(start), m_EndPoint(end)
    {
        // 创建直线
        osg::ref_ptr<osg::Geometry> lineGeometry = new osg::Geometry();
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
        vertices->push_back(start);
        vertices->push_back(end);
        lineGeometry->setVertexArray(vertices.get());
        lineGeometry->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, 2));
        // 创建一个线宽对象，并设置宽度
        osg::ref_ptr<osg::LineWidth> lineWidth = new osg::LineWidth(5.0f);
        lineGeometry->getOrCreateStateSet()->setAttribute(lineWidth.get());
        osg::ref_ptr<osg::Geode> lineGeode = new osg::Geode();
        lineGeode->addDrawable(lineGeometry.get());
        this->addChild(lineGeode);

        // 创建三个球体
        m_SphereNodes[0] = CreateSphere(start, 0.1f, COLOR_WHITE);
        m_SphereNodes[1] = CreateSphere(end, 0.1f, COLOR_WHITE);
        m_SphereNodes[2] = CreateSphere((start + end) * 0.5f, 0.1f, COLOR_WHITE);
    }

    void HighlightPoint(FeaturePoint point)
    {
        if (point < 0 || point >= m_SphereNodes.size()) return;
        HideAllPoint();

        const osg::Vec4& color = (point == CENTER_POINT) ? COLOR_YELLOW : COLOR_RED;

        auto* node = m_SphereNodes[point].get();
        node->setNodeMask(~0);
        osg::Geode* geode = dynamic_cast<osg::Geode*>(node);
        if (!geode) return;
        osg::ShapeDrawable* drawable = dynamic_cast<osg::ShapeDrawable*>(geode->getDrawable(0));
        if (!drawable) return;
        drawable->setColor(color);
        m_Highlighted = true;
    }

    void HideAllPoint()
    {
        for (auto& node : m_SphereNodes)
        {
            node->setNodeMask(0);
        }
    }

    void EraseHighlighted()
    {
        HideAllPoint();
        for (auto& node : m_SphereNodes)
        {
            osg::Geode* geode = dynamic_cast<osg::Geode*>(node.get());
            if (!geode) return;
            osg::ShapeDrawable* drawable = dynamic_cast<osg::ShapeDrawable*>(geode->getDrawable(0));
            if (!drawable) return;
            drawable->setColor(osg::Vec4(1, 1, 1, 1));
        }
        m_Highlighted = false;
    }

    void TogglePointVisibility()
    {
        if (m_Highlighted) EraseHighlighted();
        for (auto& node : m_SphereNodes)
        {
            node->setNodeMask(node->getNodeMask() ? 0 : ~0);
        }
    }

    const osg::Vec3& StartPoint() const
    {
        return m_StartPoint;
    }

    const osg::Vec3& EndPoint() const
    {
        return m_EndPoint;
    }

private:
    osg::ref_ptr<osg::Node> CreateSphere(const osg::Vec3& center, float radius, const osg::Vec4& color)
    {
        osg::ref_ptr<osg::Sphere> sphere = new osg::Sphere(center, radius);
        osg::ref_ptr<osg::ShapeDrawable> sd = new osg::ShapeDrawable(sphere);
        sd->setColor(color);

        osg::ref_ptr<osg::Geode> geode = new osg::Geode();
        geode->addDrawable(sd.get());
        geode->setNodeMask(0);
        this->addChild(geode);
        return geode; // 返回球体节点的引用
    }

    osg::Vec3 m_StartPoint;
    osg::Vec3 m_EndPoint;
    std::array<osg::ref_ptr<osg::Node>, 3> m_SphereNodes;
    bool m_Highlighted = false;
};

class KeyboardEventHandler : public osgGA::GUIEventHandler
{
public:
    KeyboardEventHandler(MySegment* segment) : _segment(segment)
    {
    }

    virtual bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa)
    {
        if (ea.getEventType() == osgGA::GUIEventAdapter::KEYDOWN)
        {
            if (ea.getKey() == 'x' || ea.getKey() == 'X')
            {
                _segment->TogglePointVisibility();
                return true;
            }
        }
        return false;
    }

private:
    MySegment* _segment;
};


class PickHandler : public osgGA::GUIEventHandler
{
public:
    virtual bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa)
    {
        if (ea.getEventType() == osgGA::GUIEventAdapter::PUSH &&
            ea.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
        {
            osgViewer::Viewer* viewer = dynamic_cast<osgViewer::Viewer*>(&aa);
            if (!viewer) return false;
            pick(viewer, ea.getX(), ea.getY());
            return true;
        }
        return false;
    }

    void pick(osgViewer::Viewer* viewer, float x, float y)
    {
        osg::ref_ptr<osgUtil::PolytopeIntersector> intersector =
            new osgUtil::PolytopeIntersector(osgUtil::Intersector::WINDOW, x - 8, y - 8, x + 8, y + 8);
        osgUtil::IntersectionVisitor iv(intersector);
        viewer->getCamera()->accept(iv);

        if (!intersector->containsIntersections()) return;
        const auto& intersection = intersector->getFirstIntersection();
        osg::Vec3 worldIntersect = intersection.localIntersectionPoint * (*intersection.matrix);

        std::cout << "Picked object at ("
            << worldIntersect.x() << ", " << worldIntersect.y() << ", " << worldIntersect.z() << ")\n";


        for (auto& node : intersection.nodePath)
        {
            MySegment* seg = dynamic_cast<MySegment*>(node);
            if (!seg) continue;
            osg::Vec3 p1 = seg->StartPoint();
            osg::Vec3 p2 = seg->EndPoint();
            osg::Vec3 center = (p1 + p2) * 0.5;

            checkProximity(MySegment::START_POINT, seg, p1, worldIntersect, 0.1, viewer);
            checkProximity(MySegment::END_POINT, seg, p2, worldIntersect, 0.1, viewer);
            checkProximity(MySegment::CENTER_POINT, seg, center, worldIntersect, 0.12, viewer);
        }
    }

    void checkProximity(MySegment::FeaturePoint feature_point, MySegment* segment, const osg::Vec3& point,
                        const osg::Vec3& hitPoint, double threshold, osgViewer::Viewer* viewer)
    {
        auto* camera = viewer->getCamera();
        // 将世界坐标转换为屏幕坐标
        osg::Vec3 screenPoint = point * camera->getViewMatrix() * camera->getProjectionMatrix();
        osg::Vec3 screenHitPoint = hitPoint * camera->getViewMatrix() * camera->getProjectionMatrix();

        double distance = (screenPoint - screenHitPoint).length();
        if (distance < threshold)
        {
            std::cout << "Close to " << (int)feature_point
                << " (" << point.x() << ", " << point.y() << ", " << point.z() << ") Distance: " << distance << "\n";

            segment->HighlightPoint(feature_point);
        }
    }
};

int main(int argc, char** argv)
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();

#ifdef OSG_GL3_AVAILABLE
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setOption(QSurfaceFormat::DebugContext);
#else
    format.setVersion(2, 0);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setOption(QSurfaceFormat::DebugContext);
#endif
    format.setDepthBufferSize(24);
    //format.setAlphaBufferSize(8);
    format.setSamples(8);
    format.setStencilBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);


    osgQOpenGLWidget widget;

    QObject::connect(&widget, &osgQOpenGLWidget::initialized, [&widget]
    {
        auto* viewer = widget.getOsgViewer();
        // set up the camera manipulators.
        {
            osg::ref_ptr<osgGA::KeySwitchMatrixManipulator> keyswitchManipulator =
                new osgGA::KeySwitchMatrixManipulator;

            keyswitchManipulator->addMatrixManipulator('1', "Trackball", new osgGA::TrackballManipulator());
            keyswitchManipulator->addMatrixManipulator('2', "Flight", new osgGA::FlightManipulator());
            keyswitchManipulator->addMatrixManipulator('3', "Drive", new osgGA::DriveManipulator());
            keyswitchManipulator->addMatrixManipulator('4', "Terrain", new osgGA::TerrainManipulator());
            keyswitchManipulator->addMatrixManipulator('5', "Orbit", new osgGA::OrbitManipulator());
            keyswitchManipulator->addMatrixManipulator('6', "FirstPerson", new osgGA::FirstPersonManipulator());
            keyswitchManipulator->addMatrixManipulator('7', "Spherical", new osgGA::SphericalManipulator());

            viewer->setCameraManipulator(keyswitchManipulator.get());
        }

        // add the state manipulator
        viewer->addEventHandler(
            new osgGA::StateSetManipulator(viewer->getCamera()->getOrCreateStateSet()));

        // add the thread model handler
        viewer->addEventHandler(new osgViewer::ThreadingHandler);
        // add the window size toggle handler
        viewer->addEventHandler(new osgViewer::WindowSizeHandler);
        // add the stats handler
        viewer->addEventHandler(new osgViewer::StatsHandler);
        // add the record camera path handler
        viewer->addEventHandler(new osgViewer::RecordCameraPathHandler);
        // add the LOD Scale handler
        viewer->addEventHandler(new osgViewer::LODScaleHandler);
        // add the screen capture handler
        viewer->addEventHandler(new osgViewer::ScreenCaptureHandler);
        // add pick event
        viewer->addEventHandler(new PickHandler());

        // 创建场景根节点
        osg::ref_ptr<osg::Group> root = new osg::Group();

        // 创建一条线段
        {
            auto start = osg::Vec3(0.0f, 0.0f, 0.0f);
            auto end = osg::Vec3(10.0f, 10.0f, 0.0f);
            osg::ref_ptr<MySegment> seg = new MySegment{start, end};

            viewer->addEventHandler(new KeyboardEventHandler{seg.get()});

            root->addChild(seg.get());
        }

        // 将节点添加到场景中
        viewer->setSceneData(root.get());

        widget.getOsgViewer()->realize();

        return 0;
    });


    widget.show();

    return app.exec();
}
