#include "mygraph.h"

#include <limits>
#include <random>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/erdos_renyi_generator.hpp>
#include <boost/random/linear_congruential.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/graph/fruchterman_reingold.hpp>
#include <boost/graph/random_layout.hpp>
#include <boost/graph/topology.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/circle_layout.hpp>
#include <boost/graph/strong_components.hpp>
#include <boost/graph/graph_utility.hpp>
#include <boost/graph/clustering_coefficient.hpp>
#include <boost/graph/exterior_property.hpp>

#include <QTime>

std::random_device rd;
std::mt19937 generator(rd());


QString globalDirPath;
quint32 global_e = 0;
quint32 global_v = 0;
static int no_run = 0;

enum RandomAgg { I_a, I_b, I_c,
                 II_a, II_a_i, II_b, II_b_i, II_c, II_d, II_e, II_f, II_g, II_h,
                 III_a, III_b, III_c, III_d, III_e,
                 GN_Clustering, CNM_Clustering,
                 R1a,
                 I_x,
                 RFD
               };

Graph::Graph()
{   //set up graphic scenes to display all kinds of stuff
    graphIsReady = false;
    generator.seed(sqrt(QTime::currentTime().msec()*QTime::currentTime().msec()));
}

Graph::~Graph()
{

    for (int i = 0; i < myEdgeList.size(); i++)
        delete myEdgeList[i];
    /*
    for (int i = 0; i < myVertexList.size(); i++)
        delete myVertexList[i];*/
    myVertexList.clear();
    myEdgeList.clear();
    for (int i = 0; i < centroids.size(); i++)
        delete centroids[i];
    centroids.clear();
    //
    ground_truth_communities.clear();
    hierarchy.clear();
    large_result.clear();
    large_excluded.clear();
    overlapped_vertices_ground_truth_cluster.clear();
}

// -------------------------------- SNAP CONVERTER/RELATED ----------------------------------
PUNGraph Graph::convertToSnapUnGraph() const
{
    PUNGraph G = TUNGraph::New();
    for (int i = 0; i < myVertexList.size(); i++)
        G->AddNode(-1);

    for (int i = 0; i < myEdgeList.size(); i++)
    {
        Edge * e = myEdgeList.at(i);
        quint32 from = e->fromVertex()->getIndex(),
                to = e->toVertex()->getIndex();
        G->AddEdge(from, to);
    }
    return G;
}

/** Convert to Snap data strucutre
 * @brief Graph::convertCommToSnapVec
 * @param CommV
 * @return true if success, false otherwise
 */
bool Graph::convertCommToSnapVec(TVec<TCnCom> &CommV)
{
   if (large_result.empty())
   {
       qDebug() << "- Ground Truth Community Has Not Been Defined!";
       return false;
   }
   for (int i = 0; i < large_result.size(); i++)
   {
       QList<quint32> comm = large_result.at(i);
       TCnCom c;
       for (int j = 0; j < comm.size(); j++)
       {
           c.Add(comm[j]);
       }
       CommV.Add(c);
   }
   return true;
}

/** Parse Result Community from Snap Graph
 * @brief Graph::convertSnapCommtoMyComm
 * @param CommV
 * @return
 */
bool Graph::convertSnapCommtoMyComm(const TVec<TCnCom> &CommV, QList<QList<quint32> > &result)
{
    if (CommV.Len() == 0)
    {
        qDebug() << "Snap Community Empty, Nothing To Do Here ...";
        return false;
    }
    for (int i = 0; i < CommV.Len(); i++)
    {
        TCnCom c = CommV[i];
        QList<quint32> comm;
        for (int j = 0; j < c.Len(); j++)
        {
            quint32 id = c[j];
            comm.append(id);
        }
        result.append(comm);
    }
    return true;
}



// ----------------------- GRAPH GENERATOR -------------------------------------------
/** Girvan and Newman Experiment
 * @brief Graph::generateHiddenGnp
 */
void Graph::generateHiddenGnp(double z_in, double z_out)
{
    if (myVertexList.size() > 0 || myEdgeList.size() > 0)
    {
        myVertexList.clear();
        myEdgeList.clear();
    }
    int n = 128, m = 4, v_per_c = n/m;
    QList<QPair<int,int> > e;
    std::uniform_real_distribution<double> dis(0,1);
    int out = 0 ,in = 0;
    for (int i = 0; i < n; i++)
    {
        for (int j = i+1; j < n; j++)
        {
            double ran = dis(generator);
            bool edge = false;
            if (i == j)
                continue;
            else if (i/v_per_c == j/v_per_c)
            {
                if (ran <= z_in)
                {
                    edge = true;
                    in++;
                }
            }
            else
            {
                if (ran <= z_out)
                {
                    edge = true;
                    out++;
                }
            }

            if (edge)
            {
                QPair<int,int> edge = qMakePair(i,j);
                e.append(edge);
            }
        }
    }

    printf("In-edge: %d\nOut-edge: %d\n", in, out);
    for (int i = 0 ; i < n; i++)
    {
        Vertex * v = new Vertex;
        v->setIndex(i);
        myVertexList.append(v);
    }

    for (int i = 0 ; i < e.size(); i++)
    {
        QPair<int,int>  edge = e.at(i);
        Edge * new_e = new Edge(myVertexList.at(edge.first),
                                myVertexList.at(edge.second),
                                i);
        myEdgeList.append(new_e);
    }
    //set the ground truth
    QList<QList<quint32> > C;
    for (int i = 0; i < m; i++)
    {
        QList<quint32> c;
        C.append(c);
    }

    for (int i = 0; i < n;i++)
    {
        quint32 c_id = i/(v_per_c);
        C[c_id].append(i);
    }

    graphIsReady = true;
    ground_truth_communities = C;
    printf("- Hidden Partition Generated: \nV: %d\nE: %d\nGround Truth Comm: %d\n",
           myVertexList.size(), myEdgeList.size(), ground_truth_communities.size());
}


/** Gnp with large N,
 * see note for more details i.e. threshold, q and p
 * let G = g.l where g is the number of hidden clusters and l is the number of vertices in cluster
 * q is the within edge probability
 * p is the out edge probability
 * @brief Graph::generateHiddenGnp_LargeN
 * @param pin
 * @param pout
 * @param n
 */

void Graph::generateHiddenGnp_LargeN(double q, double p, quint32 l)
{
    if (myVertexList.size() > 0 || myEdgeList.size() > 0)
    {
        myVertexList.clear();
        myEdgeList.clear();
    }
    int g = 4;
    QList<QPair<quint32,quint32> > e;
    std::uniform_real_distribution<double> dis(0,1);
    quint64 n = g*l;
    for (size_t i = 0; i < n; i++)
    {
        for (size_t j = i+1; j < n; j++)
        {
            double ran = dis(generator);
            bool edge = false;
            if (i == j)
                continue;
            else if (i/l == j/l) //same cluster
            {
                if (ran <= q)
                {
                    edge = true;
                }
            }
            else //diff cluster
            {
                if (ran <= p)
                {
                    edge = true;
                }
            }

            if (edge)
            {
                QPair<quint32,quint32> edge = qMakePair(i,j);
                e.append(edge);
            }
        }
    }

    for (size_t i = 0 ; i < n; i++)
    {
        Vertex * v = new Vertex;
        v->setIndex(i);
        myVertexList.append(v);
    }

    for (int i = 0 ; i < e.size(); i++)
    {
        QPair<int,int>  edge = e.at(i);
        Edge * new_e = new Edge(myVertexList.at(edge.first),
                                myVertexList.at(edge.second),
                                i);
        myEdgeList.append(new_e);
    }
    //set the ground truth
    QList<QList<quint32> > C;
    for (size_t i = 0; i < g; i++)
    {
        QList<quint32> c;
        C.append(c);
    }

    for (size_t i = 0; i < n;i++)
    {
        quint32 c_id = i/l;
        C[c_id].append(i);
    }

    graphIsReady = true;
    ground_truth_communities = C;
    printf("- Hidden Partition Generated: \nV: %d\nE: %d\nGround Truth Comm: %d\n",
           myVertexList.size(), myEdgeList.size(), ground_truth_communities.size());
}

/** Generate A Simple Cycle
 * @brief Graph::generateSimpleCycle
 * @param n
 */
void Graph::generateSimpleCycle(const int &n)
{   //generate vertices
    for(int i = 0; i < n; i++)
    {
        Vertex * v = new Vertex;
        v->setIndex(i);
        myVertexList.append(v);
    }
    //generate edges
    for(int i = 0; i <= n-2; i++)
    {
        Vertex * v = myVertexList.at(i),
               * u = myVertexList.at(i+1);
        Edge * e1 = new Edge(v,u,myEdgeList.size());
        myEdgeList.append(e1);
    }
    Edge * e = new Edge(myVertexList.first(), myVertexList.last(), myEdgeList.size());
    myEdgeList.append(e);
}

/** Generate Large Binary Tree
 * @brief Graph::generateBinaryTree
 * @param h: height of the desired tree
 *
 */
void Graph::generateBinaryTree(const int &h)
{
    unsigned int n = qPow(2,h) - 1;
    for(int i = 0; i < n; i++)
    {
        Vertex * v = new Vertex;
        v->setIndex(i);
        myVertexList.append(v);
    }
    //generate edges

    for(int i = 0; i < n; i++)
    {
        int level = qFloor(std::log2(i+1))+1;
        if (level == h) continue;
        unsigned int root = i,
                leftC = i*2 + 1,
                rightC = i*2 + 2;
        Vertex * p  = myVertexList.at(root),
               * l = myVertexList.at(leftC),
               * r = myVertexList.at(rightC);
        Edge * e1 = new Edge(p,l,myEdgeList.size()),
             * e2 = new Edge(p,r,myEdgeList.size());
        myEdgeList.append(e1);
        myEdgeList.append(e2);
    }

}

/** READ GML FILE AND PARSE FOR EDGE FILE
 * Otherwise GML takes way too long to parse
 * @brief Graph::read_GML_file
 * @param filePath
 */
void Graph::read_GML_file(QString filePath)
{
    QFile file(filePath);
    if (!file.exists())
    {
        qDebug() << "File Not Found";
        return;
    }
    GMLpath = filePath;
    QFileInfo fileInfo(file);
    QDir dir = fileInfo.absoluteDir();
    globalDirPath = dir.absolutePath();
    if (!file.exists())
    {
        qDebug() << "FILE NOT FOUND";
        return;
    }

    file.open(QFile::ReadOnly | QFile::Text);
    QTextStream in(&file);
    QString all = in.readAll();
    QStringList split = all.split("\n");
    QString directed = split[3];
    if (!directed.contains("0"))
    {
        qDebug() << "DIRECTED GRAPH";
        return; //directed graph
    }
    QSet<int> v;
    QList<QPair<int,int> > my_edge;
    QStringListIterator iter(split);
    while(iter.hasNext())
    {
        QString str = iter.next();
        if (str.contains("node") || str.contains("edge"))
        {
            bool node = str.contains(" node");
            bool edge = str.contains(" edge");
            if (!node && !edge)
            {
                qDebug() << "FILE FORMATE ERROR";
                return;
            }
            else
                iter.next();
            if (node)
            {
                while (iter.peekNext() != "  ]")
                {
                    QStringList cur = iter.next().split(" ");
                    if (cur.contains("id"))
                    {
                        bool ok;
                        QStringList string = cur.last().split(".");
                        QString index = string.first();
                        int i = index.toInt(&ok);
                        if (ok)
                        {
                            Vertex * v = new Vertex;
                            v->setIndex(i);
                            myVertexList.append(v);
                        }
                        else
                        {
                            qDebug() << "Error While Parsing GML: id not recognised!";
                            return;
                        }
                    }
                    else if (cur.contains("label"))
                    {
                        int index = cur.indexOf("label");
                        index++;
                        QString label;
                        for (index; index < cur.size(); index++)
                        {
                            label.append(cur.at(index));
                        }
                    }
                    else if (cur.contains("value"))
                    {
                        bool ok;
                        //vertex_weight.append(cur.last().toInt(&ok));
                        if (!ok)
                        {
                            QString c = cur.last();
                            //QString name = vertex_label.last();
                            QString parsed_name = "";
                           // for (int i = 1; i < name.size()-1;i++) parsed_name += name[i];
                        }
                    }
                }
            }
            else if (edge)
            {
                int source = -1, des = -1;
                while (iter.peekNext() != "  ]")
                {
                    QStringList cur = iter.next().split(" ");
                    bool ok;
                    if (cur.contains("source"))
                    {
                        QStringList string = cur.last().split(".");
                        QString index = string.first();
                        source = index.toInt(&ok);
                    }
                    else if (cur.contains("target"))
                    {
                        QStringList string = cur.last().split(".");
                        QString index = string.first();
                        des = index.toInt(&ok);
                    }
                    else if (cur.contains("value"))
                    {
                      //  edge_weight.append(cur.last().toInt());
                    }
                    if (!ok)
                    {
                        qDebug() << QString("- Error While Loading Edge: e(%1, %2)").arg(source).arg(des);
                    }
                }
                if (source != -1 && des != -1)
                {
                    QPair<int,int> e = qMakePair(source,des);
                    my_edge.append(e);
                }
                else
                {
                    qDebug() << "EDGE ERROR" << source << des;
                    return;
                }
            }
            else
            {
                qDebug() << "ERROR: UNKNOWN ERROR WHILE PARSING! RETURNING";
                return;
            }

        }
        else
            continue;
    }

    for (int i = 0; i < my_edge.size(); i++)
    {
        QPair<int,int> e = my_edge[i];
        int v = e.first, u = e.second;
        if (v >= myVertexList.size() || u >= myVertexList.size())
        {
            qDebug() << " V - Index Out of Range!" << v << u;
            qDebug() << myVertexList.size();
            return;
        }
        Vertex * from = myVertexList[v];
        Vertex * to = myVertexList[u];
        Edge * newe = new Edge(from,to,i);
        myEdgeList.append(newe);
    }

    qDebug() << "GML Parsed Successfully!";
    qDebug() << "V: " << myVertexList.size() << "; E: " << myEdgeList.size();
    save_edge_file_from_GML();
}

/** Load Hierarchy Ground Truth from LFR
 * @brief Graph::load_LFR_groundTruth
 */
void Graph::load_LFR_groundTruth()
{
    QDir dir(globalDirPath);
    QFileInfoList fileList = dir.entryInfoList();
    QString first, second;
    bool def = true;
    for (int i = 0; i < fileList.size(); i++)
    {
        QFileInfo f = fileList.at(i);
        QString filename = f.fileName();
        if (filename.contains("communit"))
        {
            if (filename.contains("first"))
            {
                first = f.absoluteFilePath();
                def = false;
            }
            else if (filename.contains("second"))
            {
                second = f.absoluteFilePath();
                def = false;
            }
        }
    }
    if (def)    parse_LFR_groundTruth();
    else    parse_LFR_groundTruth(second, 2);
}

void Graph::load_LFR_graph()
{
    QDir dir(globalDirPath);
    if (!dir.exists())
    {
        qDebug() << "DIR NOT EXISTS! Terminating ...";
        return;
    }
    QStringList filters;
    filters << "*.dat";
    QFileInfoList file = dir.entryInfoList(filters);
    QString e_file;
    for (int i = 0; i < file.size(); i++)
    {
        QFileInfo f = file.at(i);
        QString name = f.fileName();
        if (name.contains("network"))
            e_file = f.absoluteFilePath();
    }

    //reload original vertices
    //Parsing
    QFile efile(e_file);
    if (!efile.exists())
    {
        qDebug() << "FILE NOT FOUND! Recheck! Terminating ...";
        return;
    }
    //else
    //READ E FILE
    efile.open(QFile::ReadOnly | QFile::Text);
    QTextStream ein(&efile);
    QList<QPair<quint32,quint32> > edge;
    while (!ein.atEnd())
    {
        QStringList str = ein.readLine().split('\t');
        if (str[0].startsWith("#")) continue;
        bool ok;
        quint32 v1 = str[0].toUInt(&ok), v2 = str[1].toUInt(&ok);
        //index starts from 1 instead of 0
        v1--;
        v2--;
        if (ok)
        {
            edge.append(qMakePair(v1,v2));
        }
    }
    efile.close();
    qDebug() << "Generating Edges ...";
    typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS> NormalGraph;
    NormalGraph g;
    for (int i = 0; i < edge.size(); i++)
    {
        QPair<quint32,quint32> p = edge.at(i);
        boost::add_edge(p.first, p.second, g);
    }
    global_v = boost::num_vertices(g);
    global_e = boost::num_edges(g);
    // adding ve edge independent of global file
    for (int i = 0; i < global_v; i++ )
    {
        Vertex * v = new Vertex;
        v->setIndex(i);
        myVertexList.append(v);
    }

    for (int i = 0; i < edge.size(); i++)
    {
        QPair<quint32,quint32> e = edge[i];
        quint32 v = e.first, u = e.second;
        Vertex * from = myVertexList.at(u);
        Vertex * to = myVertexList.at(v);
        Edge * newe = new Edge(from,to,i);
        myEdgeList.append(newe);
    }


    bool fit = false;
    if (myVertexList.size() == global_v && myEdgeList.size() == global_e)
        fit = true;
    qDebug() << "Check Sum" << fit;
    if (fit)
    {
        graphIsReady = true;
        parse_LFR_groundTruth();
        save_current_run_as_edge_file("edge_file.txt");
    }
    else
    {
        qDebug() << "Preset V: " << global_v << "; E: " << global_e;
        qDebug() << "Load V: " << myVertexList.size() << "; E: " << myEdgeList.size();
    }
}

/** DEFAULT PARSER OF LFR: READ FILE "comunity.dat"
 * @brief Graph::parse_LFR_groundTruth
 */
void Graph::parse_LFR_groundTruth()
{
    //read ground truth first
    QString fileName("community.dat");
    if (!locate_file_in_dir(fileName))
    {
        qDebug() << "FILE NOT FOUND!";
        return;
    }
    QFile truth(fileName);
    if (!truth.exists())
    {
        qDebug() << "Truth File Not Found for LFR!";
        return;
    }
    truth.open(QFile::ReadOnly | QFile::Text);
    QTextStream in(&truth);
    QSet<int> C;
    while (!in.atEnd())
    {
        QStringList line = in.readLine().split('\t');
        bool ok;
        quint32 vi = line[0].toUInt(&ok),
                comm = line[1].toUInt(&ok);
        //truth file starts from 1
        vi -= 1;
        comm -= 1;
        if (!ok)
        {
            qDebug() << "- Something went wrong wile parsing LFR ground truth";
            return;
        }
        else
        {
            Vertex * v = myVertexList.at(vi);
            v->setTruthCommunity(comm);
            if (!C.contains(comm))
                C.insert(comm);
        }
    }
    //set ground truth communities
    for (int i = 0; i < C.size(); i++)
    {
        QList<quint32> c;
        ground_truth_communities.append(c);
    }

    for (int i = 0; i < myVertexList.size(); i++)
    {
        Vertex * v = myVertexList.at(i);
        quint32 c = v->getTruthCommunity();
        ground_truth_communities[c].append(v->getIndex());
    }

    assign_vertex_to_its_ground_truth_comm();
}

/** READ LFR COMMUNITY PATH FOR HIERARCHY
 * @brief Graph::parse_LFR_groundTruth
 * @param filepath
 * @param level: hierarchy level
 */
void Graph::parse_LFR_groundTruth(QString filepath, int level)
{
    //read ground truth first
    QFile truth(filepath);
    if (!truth.exists())
    {
        qDebug() << "Truth File Not Found for LFR!";
        return;
    }
    truth.open(QFile::ReadOnly | QFile::Text);
    QTextStream in(&truth);
    QSet<int> C;
    while (!in.atEnd())
    {
        QStringList line = in.readLine().split('\t');
        bool ok;
        quint32 vi = line[0].toUInt(&ok),
                comm = line[1].toUInt(&ok);
        //truth file starts from 1
        vi -= 1;
        comm -= 1;
        if (!ok)
        {
            qDebug() << "- Something went wrong wile parsing LFR ground truth";
            return;
        }
        else
        {
            Vertex * v = myVertexList.at(vi);
            v->setTruthCommunity(comm);
            if (!C.contains(comm))
                C.insert(comm);
        }
    }
    //set ground truth communities
    for (int i = 0; i < C.size(); i++)
    {
        QList<quint32> c;
        ground_truth_communities.append(c);
    }

    for (int i = 0; i < myVertexList.size(); i++)
    {
        Vertex * v = myVertexList.at(i);
        quint32 c = v->getTruthCommunity();
        ground_truth_communities[c].append(v->getIndex());
    }

    assign_vertex_to_its_ground_truth_comm();

    QString subdir = globalDirPath.append(QString("/GraphML/"));
    QDir dir(subdir);
    if (!dir.exists())
        QDir().mkdir(subdir);
    QString f_to_write = subdir.append(QString("LFR_community_level%1.graphml").arg(level));
    print_multiple_communities_inGraphML(f_to_write);
}


/** ASSIGN VERTEX TO ITS GROUND TRUTH AND CONSIDER ITS AS THE RESULT I.E FOR Q
 * @brief Graph::assign_vertex_to_its_ground_truth_comm
 */

void Graph::assign_vertex_to_its_ground_truth_comm()
{
    quint32 no_c = ground_truth_communities.size();
    for (int i = 0; i < no_c; i++)
    {
        QList<quint32> c;
        large_result.append(c);
    }

    for (int i = 0; i < myVertexList.size(); i++)
    {
        Vertex * v = myVertexList.at(i);
        quint32 c = v->getTruthCommunity();
        large_result[c].append(v->getIndex());
    }
}

/** Read Graph from Gephi, and collapsed associated vertices into clusters
 * to produce SuperGraph for recursive partitioning
 * @brief Graph::read_Gephi_graph_and_produce_super_graph
 * @param filePath
 */
void Graph::read_Gephi_graph_and_produce_super_graph(QString filePath)
{
    QFile file(filePath);
    if (!file.exists())
    {
        qDebug() << "File Not Found";
        return;
    }
    GMLpath = filePath;
    QString str = filePath.split(".").at(0);
    QString lvl = str.at(str.length()-1);
    bool ok;
    no_run = lvl.toUInt(&ok);
    if(!ok)
        qDebug() << "- While Parsing Gephi File: File Missing Level!";

    if (!file.exists())
    {
        qDebug() << "FILE NOT FOUND";
        return;
    }

    file.open(QFile::ReadOnly | QFile::Text);
    QTextStream in(&file);
    QString all = in.readAll();
    QStringList split = all.split("\n");
    QString directed = split[3];
    if (!directed.contains("0"))
    {
        qDebug() << "DIRECTED GRAPH";
        return; //directed graph
    }

    QList<QPair<quint32,quint32> > my_edge;
    quint32 highestClassId = 0;
    QStringListIterator iter(split);
    QMap<quint32, QString> clusterColourCode;
    while(iter.hasNext())
    {
        QString str = iter.next();
        QString rgb;
        if (str.contains("node") || str.contains("edge"))
        {
            bool node = str.contains(" node");
            bool edge = str.contains(" edge");
            if (!node && !edge)
            {
                qDebug() << "FILE FORMAT ERROR";
                return;
            }
            else
                iter.next();
            if (node)
            {
                while (iter.peekNext() != "  ]")
                {
                    QStringList cur = iter.next().split(" ");
                    if (cur.contains("id"))
                    {
                        bool ok;
                        QStringList id = cur.last().split(".");
                        quint32 i = id[0].toUInt(&ok);
                        if (ok)
                        {
                            Vertex * v = new Vertex;
                            v->setIndex(i);
                            myVertexList.append(v);
                        }
                        else
                            qDebug() << "Error While Parsing GML: id not recognised!";
                    }
                    else if (cur.contains("fill"))
                    {
                        QStringList c = cur.last().split("\"");
                        bool ok;
                        rgb = c[1];
                    }
                    else if (cur.contains("Weight"))
                    {
                        QStringList c = cur.last().split("\"");
                        bool ok;
                        quint32 w = c[1].toUInt(&ok);
                        if (!ok)
                            qDebug() << " - While Assigning Cluster to Vertex: cluster id error";
                        else
                        {
                            if (myVertexList.last()->getcSize() == 0)
                                myVertexList.last()->setcSize(w);
                        }
                    }
                    else if (cur.contains("ModularityClass") || cur.contains("Cluster"))
                    {
                        QStringList c = cur.last().split("\"");
                        bool ok;
                        quint32 clus = c[1].toUInt(&ok);
                        if (!ok)
                            qDebug() << " - While Assigning Cluster to Vertex: cluster id error";
                        else
                        {

                            if (myVertexList.last()->getTruthCommunity() < 0)
                                myVertexList.last()->setTruthCommunity(clus);
                            if (clus > highestClassId)
                                highestClassId = clus;
                            if (rgb.size() == 0)
                            {
                                qDebug() << "- While Parsing Gephi GML: Error Colour Has Not Been Set!";
                            }
                            else
                            {
                                if(!clusterColourCode.contains(clus))
                                {
                                    clusterColourCode.insert(clus,rgb);
                                }
                            }
                        }
                    }
                }
            }
            else if (edge)
            {
                //quint32 max = std::numeric_limits<quint32>::max();
                quint32 max = 99999;
                quint32 source = max, des = max;
                while (iter.peekNext() != "  ]")
                {
                    QStringList cur = iter.next().split(" ");
                    if (cur.contains("source"))
                    {
                        QStringList id = cur.last().split(".");
                        source = id[0].toUInt();
                    }
                    else if (cur.contains("target"))
                    {
                        QStringList id = cur.last().split(".");
                        des = id[0].toUInt();
                    }
                }
                if (source != max && des != max)
                {
                    QPair<quint32,quint32> e = qMakePair(source,des);
                    QPair<quint32,quint32> reverse = qMakePair(des,source);
                    if (!my_edge.contains(e) && !my_edge.contains(reverse))
                    {
                        my_edge.append(e);
                        my_edge.append(reverse);
                    }
                    else
                        qDebug() << "Error While Parsing GML: Duplicate Edge!";
                }
                else
                {
                    qDebug() << "EDGE ERROR" << source << des;
                    return;
                }
            }
            else
            {
                qDebug() << "ERROR: UNKNOWN ERROR WHILE PARSING! RETURNING";
                return;
            }

        }
        else
            continue;
    }

    for (int i = 0; i < my_edge.size(); i+= 2)
    {
        QPair<quint32,quint32> e = my_edge[i];
        quint32 v = e.first, u = e.second;
        if (v > myVertexList.size() || u > myVertexList.size())
            qDebug() << v << u;
        Vertex * from = myVertexList[v];
        Vertex * to = myVertexList[u];
        Edge * newe = new Edge(from,to,i/2);
        myEdgeList.append(newe);
    }
    //reindexing the colour code
    //code -> colour
    qDebug() << "GML Parsed Successfully!";
    qDebug() << "V: " << myVertexList.size() << "; E: " << myEdgeList.size();
    qDebug() << "No Cluster: " << highestClassId+1;
    qDebug() << "No Colour: " << clusterColourCode.size();
    Gephi_parse_ModularityClass(highestClassId+1);
    PostAgg_generate_super_vertex();
    print_result_community__with_attributes_inGraphML(clusterColourCode);
}

/** Put Vertices with same ModularityClass in the same cluster and aggregate them
 * @brief Graph::Gephi_parse_ModularityClass
 */
void Graph::Gephi_parse_ModularityClass(int noClass)
{
    QList<QList<quint32> > clusters;
    for (int i = 0; i < noClass; i++)
    {
        QList<quint32> c;
        clusters << c;
    }
    for(int i = 0; i < myVertexList.size(); i++)
    {
        Vertex * v = myVertexList.at(i);
        quint32 clus = v->getTruthCommunity();
        if (clus > clusters.size())
        {
            qDebug() << "- While Parsing Gephi:\n - While Assigning Clusters to Vertices: ClusterID Error";
            return;
        }
        else
        {
            clusters[clus].append(v->getIndex());
        }
    }
    large_result = clusters;
    clusters.clear();
    //check sum
}


/** Save GML file as Simple .txt File for faster parsing
 * @brief Graph::save_edge_file_from_GML
 */
void Graph::save_edge_file_from_GML()
{
    qDebug() << "No Problem Detected! Saving Edge File";
    if (GMLpath.size() == 0)
    {
        qDebug() << "GML Path Has Not Been Set";
    }
    else
    {
        QFileInfo info(GMLpath);
        QDir dir = info.absoluteDir();
        QFile vFile(dir.absolutePath() + "/vertex_file.txt");
        vFile.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream os(&vFile);
        os << QString::number(myVertexList.size()) << '\t' << QString::number(myEdgeList.size()) << endl;
        vFile.close();

        QFile outFile(dir.absolutePath() + "/edge_file.txt");
        outFile.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream ts(&outFile);
        for (int i = 0; i < myEdgeList.size(); i++)
        {
            int from = myEdgeList[i]->fromVertex()->getIndex(), to = myEdgeList[i]->toVertex()->getIndex();
            ts << QString::number(from) << '\t' << QString::number(to) << endl;
        }
        outFile.close();
    }
}

/** Just Save the Edge to current Dir
 * @brief Graph::save_current_run_as_edge_file
 */
void Graph::save_current_run_as_edge_file(QString fileName)
{
    QString absolutePath = globalDirPath + fileName;
    QFile outFile(absolutePath);
    if (outFile.exists())
        outFile.remove();
    outFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream ts(&outFile);

    for (int i = 0; i < myEdgeList.size(); i++)
    {
        quint32 from = myEdgeList[i]->fromVertex()->getIndex(), to = myEdgeList[i]->toVertex()->getIndex();
        ts << QString::number(from) << '\t' << QString::number(to) << endl;
    }
    outFile.close();
}

void Graph::save_current_run_summary_file(QString fileName)
{
    QFile outFile(fileName);
    if (outFile.exists())
        outFile.remove();
    outFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream ts(&outFile);
    ts << "Vertex\tEdge" << endl;
    ts << QString::number(myVertexList.size()) << "\t" << QString::number(myEdgeList.size()) << endl;
    outFile.close();
}

/**
 * @brief Graph::read_simple_edge
 * @param dirPath
 */
void Graph::read_simple_edge(QString dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists())
    {
        qDebug() << "DIR NOT EXISTS! Terminating ...";
        return;
    }
    globalDirPath = dirPath;
    QStringList filters;
    filters << "*.txt";
    QFileInfoList file = dir.entryInfoList(filters);
    QString v_file, e_file;
    for (int i = 0; i < file.size(); i++)
    {
        QFileInfo f = file.at(i);
        QString name = f.fileName();
        if (name.contains("edge"))
            e_file = f.absoluteFilePath();
        else if (name.contains("vertex"))
            v_file = f.absoluteFilePath();
        else
        {
            qDebug() << "While READING FILES: File not Recognised!";
            qDebug() << file[i].fileName() << " Skipping this File";
        }
    }

    //reload original vertices
    //Parsing
    QFile efile(e_file), vfile(v_file);
    if (!efile.exists() || !vfile.exists())
    {
        qDebug() << "FILE NOT FOUND! Recheck! Terminating ...";
        return;
    }
    //else
    vfile.open(QFile::ReadOnly | QFile::Text);
    QTextStream vin(&vfile);
    QStringList str = vin.readLine().split('\t');
    bool load;
    global_v = str[0].toUInt(&load);
    global_e = str[1].toUInt(&load);
    if (!load)
    {
        qDebug() << "ERROR LOADING V FILE";
        return;
    }
    qDebug() << "Graph: " << "V: " <<  global_v << "; E: " << global_e;
    vfile.close();
    //READ E FILE
    efile.open(QFile::ReadOnly | QFile::Text);
    QTextStream ein(&efile);
    QList<QPair<quint32,quint32> > edge;
    while (!ein.atEnd())
    {
        QStringList str = ein.readLine().split('\t');
        if (str[0].startsWith("#")) continue;
        bool ok;
        quint32 v1 = str[0].toUInt(&ok), v2 = str[1].toUInt(&ok);
        if (ok)
        {
            edge.append(qMakePair(v1,v2));
        }
    }
    efile.close();
    qDebug() << "Generating Vertex and Edges ...";
    // adding ve edge independent of global file
    for (int i = 0; i < global_v; i++ )
    {
        Vertex * v = new Vertex;
        v->setIndex(i);
        myVertexList.append(v);
    }

    for (int i = 0; i < edge.size(); i++)
    {
        QPair<quint32,quint32> e = edge[i];
        quint32 v = e.first, u = e.second;
        Vertex * from = myVertexList[v];
        Vertex * to = myVertexList[u];
        Edge * newe = new Edge(from,to,i);
        myEdgeList.append(newe);
    }


    bool fit = false;
    if (myVertexList.size() == global_v && myEdgeList.size() == global_e)
        fit = true;
    qDebug() << "Check Sum" << fit;
    if (fit)
    {
        graphIsReady = true;
    }
    else
    {
        qDebug() << "Preset V: " << global_v << "; E: " << global_e;
        qDebug() << "Load V: " << myVertexList.size() << "; E: " << myEdgeList.size();
    }
    generate_base_graph_file(dirPath);
}

void Graph::read_edge(QString dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists())
    {
        qDebug() << "DIR NOT EXISTS! Terminating ...";
        return;
    }
    globalDirPath = dirPath;
    QStringList filters;
    filters << "*.txt";
    QFileInfoList file = dir.entryInfoList(filters);
    QString e_file;
    for (int i = 0; i < file.size(); i++)
    {
        QFileInfo f = file.at(i);
        QString name = f.fileName();
        if (name.contains("edge"))
            e_file = f.absoluteFilePath();
    }

    //reload original vertices
    //Parsing
    QFile efile(e_file);
    if (!efile.exists())
    {
        qDebug() << "FILE NOT FOUND! Recheck! Terminating ...";
        return;
    }
    //else
    //READ E FILE
    efile.open(QFile::ReadOnly | QFile::Text);
    QTextStream ein(&efile);
    QList<QPair<quint32,quint32> > edge;
    while (!ein.atEnd())
    {
        QStringList str = ein.readLine().split('\t');
        if (str[0].startsWith("#")) continue;
        bool ok;
        quint32 v1 = str[0].toUInt(&ok), v2 = str[1].toUInt(&ok);
        if (ok)
        {
            edge.append(qMakePair(v1,v2));
        }
    }
    efile.close();
    qDebug() << "Generating Edges ...";
    typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS> NormalGraph;
    NormalGraph g;
    for (int i = 0; i < edge.size(); i++)
    {
        QPair<quint32,quint32> p = edge.at(i);
        boost::add_edge(p.first, p.second, g);
    }
    global_v = boost::num_vertices(g);
    global_e = boost::num_edges(g);
    // adding ve edge independent of global file
    for (int i = 0; i < global_v; i++ )
    {
        Vertex * v = new Vertex;
        v->setIndex(i);
        myVertexList.append(v);
    }

    for (int i = 0; i < edge.size(); i++)
    {
        QPair<quint32,quint32> e = edge[i];
        quint32 v = e.first, u = e.second;
        Vertex * from = myVertexList.at(u);
        Vertex * to = myVertexList.at(v);
        Edge * newe = new Edge(from,to,i);
        myEdgeList.append(newe);
    }


    bool fit = false;
    if (myVertexList.size() == global_v && myEdgeList.size() == global_e)
        fit = true;
    qDebug() << "Check Sum" << fit;
    if (fit)
    {
        graphIsReady = true;
    }
    else
    {
        qDebug() << "Preset V: " << global_v << "; E: " << global_e;
        qDebug() << "Load V: " << myVertexList.size() << "; E: " << myEdgeList.size();
    }
}

/** Load Ground Truth Communities and Parse it
 * @brief Graph::load_ground_truth_communities
 */
void Graph::load_ground_truth_communities()
{
    qDebug() << "PARSING GROUND TRUTH COMMUNITIES";
    ground_truth_communities.clear();
    QString file("truth_file.txt");
    if (!locate_file_in_dir(file))
    {
        qDebug() << "- Truth File Not Found! Load Ground Truth Fails, Terminating ...";
        return;
    }
    else
    {
        QFile toRead(file);
        toRead.open(QIODevice::ReadOnly | QIODevice::Text);
        QTextStream in(&toRead);
        while (!in.atEnd())
        {
            QStringList str = in.readLine().split('\n'); //a community
            QStringList sub_str = str[0].split('\t'); //community split by '\t'
            QList<quint32> community;
            for (int i = 0; i < sub_str.size(); i++)
            {
                bool ok;
                quint32 indx = sub_str[i].toUInt(&ok);
                if (indx == 0){}
                else if (!ok)
                {
                    qDebug() << "- Error While Parsing Ground Truth: Index Not Found!";
                    qDebug() << "- At Index: " << indx;
                    return;
                }
                community.append(indx);
            }
            ground_truth_communities.append(community);
        }
        //readjust if index does not start from
        qDebug() << "FINISHED! Number of Comm: " << ground_truth_communities.size();
        //load succesfull
        //now parsing
        qDebug() << "Now Parsing ...";
        toRead.close();
        remove_excluded_vertices_from_ground_truth();
        large_process_overlap_by_seperate_intersection();
    }
}


/** Generate a Base Graph file:
 * summary_SuperGraph0: vertex file
 * superGraph0 edge file
 * @brief Graph::generate_base_graph_file
 */
void Graph::generate_base_graph_file(QString dirPath)
{
    QDir dir(dirPath);
    QFileInfoList list = dir.entryInfoList();
    bool summary_found = false, super_found = false;
    QString edgeFilePath = "";
    for (int i = 0; i < list.size(); i++)
    {
        QFileInfo file = list[i];
        QString filename = file.fileName();
        if (filename == "summary_superGraph0.txt")
            summary_found = true;
        if (filename == "superGraph0.txt")
            super_found = true;
        if (filename == "edge_file.txt")
            edgeFilePath = file.absoluteFilePath();
    }
    if (!summary_found)
    {
        QFile summary(dirPath + "summary_superGraph0.txt");
        summary.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out(&summary);
        out << "Vertex\tEdge" << endl;
        out << QString::number(myVertexList.size()) << "\t" << QString::number(myEdgeList.size()) << endl;
    }
    if (!super_found)
    {
        QString superGraph(dirPath + "superGraph0.txt");
        QFile super(superGraph);
        super.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out(&super);
        out << "Source\tTarget" << endl;
        for (int i = 0; i < myEdgeList.size(); i++)
        {
            Edge * e = myEdgeList.at(i);
            quint32 from = e->fromVertex()->getIndex(),
                    to = e->toVertex()->getIndex();
            out << QString("%1\t%2\n").arg(from).arg(to);
        }
        qDebug() << "- Base Graph Created Successfully!";

    }
}
/*
 * RECONNECT THE GRAPH AFTER AN AGGREGATION HAS BEEN DONE
 */
void Graph::reConnectGraph()
{
    LARGE_reload();
    return;
}

void Graph::clear_edge()
{
    for(int i = 0 ; i < myVertexList.size(); i++)
        myVertexList.at(i)->removeAll();
    for(int i = 0; i < myEdgeList.size(); i++)
        delete myEdgeList[i];
    myEdgeList.clear();
}

// -----------------------------RANDOM AGGREGATE CLUSTERING -------------------------
// ----------------------------------------------------------------------------------
/** Type I.a - Uniform (Everything is Uniformly at Random)
 * @brief Graph::random_aggregate
 */
void Graph::random_aggregate()
{
    qDebug() << "CHECKING CONDITION || RECONNECTING GRAPH";
    qDebug() << "Graph Condition: " << graphIsReady <<";"<<myVertexList.size();
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;

    quint32 t = 0;
    QTime t0;
    t0.start();
    qDebug() << "STARTING...";
    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        quint32 size = players.size();
        std::uniform_int_distribution<quint32> distribution(0,size-1);
        quint32 selected_index = distribution(generator);
        Vertex * selected = players.at(selected_index);
        if (selected->is_vertex_absorbed() || selected->getParent() != 0)
        {
            qDebug() << "Candidate Has Been Clustered!!!!! ";
            return;
        }
        //get a neighbour
        quint32 no_neighbour = selected->getNumberEdge();
        if (no_neighbour == 0) // if there is no neighbour, declare a winner
        {
            winners.append(selected);
            players.removeOne(selected);
            t++;
        }
        else // else absorb
        {
            std::uniform_int_distribution<quint32> distribution2(0,no_neighbour-1);
            quint32 selected_edge_index = distribution2(generator);
            Edge * e = selected->getEdge(selected_edge_index);
            Vertex * neighbour = selected->get_neighbour_fromEdge(e); //get the neighbour (not clean)
            hierarchy.append(qMakePair(neighbour->getIndex(), selected->getIndex()));
            selected->absorb_removeEdge(e);
            players.removeOne(neighbour);
            t++;
        }
    }
    record_time_and_number_of_cluster(RandomAgg::I_a,t0.elapsed(),winners.size());
    centroids = winners;
    qDebug("I.a - Time elapsed: %d ms", t0.elapsed());
    large_graph_parse_result();
}


/** Ia_i The Inverse of Reverse Random Aggregate
 * @brief Graph::reverse_random_aggregate
 */

void Graph::reverse_random_aggregate()
{
    qDebug() << "CHECKING CONDITION || RECONNECTING GRAPH";
    qDebug() << "Graph Condition: " << graphIsReady <<";"<<myVertexList.size();
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;

    quint32 t = 0;
    QTime t0;
    t0.start();
    qDebug() << "I.a_(i) STARTING...";
    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        quint32 size = players.size();
        std::uniform_int_distribution<quint32> distribution(0,size-1);
        quint32 selected_index = distribution(generator);
        Vertex * selected = players.at(selected_index);
        if (selected->is_vertex_absorbed() || selected->getParent() != 0)
        {
            qDebug() << "Candidate Has Been Clustered!!!!! ";
            return;
        }
        //get a neighbour
        quint32 no_neighbour = selected->getNumberEdge();
        if (no_neighbour == 0) // if there is no neighbour, declare a winner
        {
            winners.append(selected);
            players.removeOne(selected);
            t++;
        }
        else // else absorb
        {
            std::uniform_int_distribution<quint32> distribution2(0,no_neighbour-1);
            quint32 selected_edge_index = distribution2(generator);
            Edge * e = selected->getEdge(selected_edge_index);
            Vertex * neighbour = selected->get_neighbour_fromEdge(e); //get the neighbour (not clean)
            hierarchy.append(qMakePair(neighbour->getIndex(), selected->getIndex()));
            neighbour->absorb_removeEdge(e);
            players.removeOne(selected);
            t++;
        }
    }
    record_time_and_number_of_cluster(RandomAgg::R1a,t0.elapsed(),winners.size());
    centroids = winners;
    qDebug("I.a_i - Time elapsed: %d ms", t0.elapsed());
    large_graph_parse_result();
}

/** Type I.b - Uniformly and Comparing the CURRENT DEGREE
 * Pr(v) = u.a.r
 * Pr(u) = u.a.r
 * Graph Type: Destructive
 * @brief Graph::random_aggregate_with_degree_comparison
 */
void Graph::random_aggregate_with_degree_comparison()
{
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }

    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;
    quint32 t = 0;
    QTime t0;
    t0.start();

    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        quint32 size = players.size();
        std::uniform_int_distribution<quint32> distribution(0,size-1);
        quint32 selected_index = distribution(generator);
        Vertex * selected = players.at(selected_index);
        //get a neighbour
        quint32 no_neighbour = selected->getNumberEdge();
        if (no_neighbour == 0) // if there is no neighbour, declare a winner
        {
            winners.append(selected);
            players.removeOne(selected);
            t++;
        }
        else // else absorb
        {
            std::uniform_int_distribution<quint32> distribution2(0,no_neighbour-1);
            quint32 selected_edge_index = distribution2(generator);
            Edge * e = selected->getEdge(selected_edge_index);
            Vertex * neighbour = selected->get_neighbour_fromEdge(selected_edge_index); //get the neighbour (not clean)
            Vertex * winner, * loser;
            quint32 selected_d = selected->getNumberEdge(), neighbour_d = neighbour->getNumberEdge();
            if (selected_d >= neighbour_d)
            {
                winner = selected;
                loser = neighbour;
            }
            else
            {
                winner = neighbour;
                loser = selected;
            }

            //abosbr
            hierarchy.append(qMakePair(loser->getIndex(), winner->getIndex()));
            winner->absorb_removeEdge(e);
            players.removeOne(loser);
            t++;
        }
    }
    record_time_and_number_of_cluster(RandomAgg::I_b,t0.elapsed(),winners.size());
    centroids = winners;
    qDebug("I.b - Time elapsed: %d ms", t0.elapsed());
    large_graph_parse_result();
}

/** REVERSE I.b or I.x
 * Pr(v) = u.a.r
 * Pr(u) = u.a.r
 * if d(v) \geq d(u) ...
 * @brief Graph::reverse_random_aggregate_with_degree_comparison
 */
void Graph::reverse_random_aggregate_with_degree_comparison()
{
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }

    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;
    quint32 t = 0;
    QTime t0;
    t0.start();

    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        quint32 size = players.size();
        std::uniform_int_distribution<quint32> distribution(0,size-1);
        quint32 selected_index = distribution(generator);
        Vertex * selected = players.at(selected_index);
        //get a neighbour
        quint32 no_neighbour = selected->getNumberEdge();
        if (no_neighbour == 0) // if there is no neighbour, declare a winner
        {
            winners.append(selected);
            players.removeOne(selected);
            t++;
        }
        else // else absorb
        {
            std::uniform_int_distribution<quint32> distribution2(0,no_neighbour-1);
            quint32 selected_edge_index = distribution2(generator);
            Edge * e = selected->getEdge(selected_edge_index);
            Vertex * neighbour = selected->get_neighbour_fromEdge(selected_edge_index); //get the neighbour (not clean)
            Vertex * winner, * loser;
            quint32 selected_d = selected->getNumberEdge(), neighbour_d = neighbour->getNumberEdge();
            if (selected_d > neighbour_d)
            {
                winner = neighbour;
                loser = selected;
            }
            else
            {
                winner = selected;
                loser = neighbour;
            }

            //abosbr
            hierarchy.append(qMakePair(loser->getIndex(), winner->getIndex()));
            winner->absorb_removeEdge(e);
            players.removeOne(loser);
            t++;
        }
    }
    record_time_and_number_of_cluster(RandomAgg::I_x,t0.elapsed(),winners.size());
    centroids = winners;
    qDebug("I.x - Time elapsed: %d ms", t0.elapsed());
    large_graph_parse_result();
}


/** Type I.c - Uniformly and Comparing the ORIGINAL DEGREE
 * Pr(v) = u.a.r
 * Pr(u) = u.a.r
 * Graph Type: Destructive
 * @brief Graph::random_aggregate_with_weight_comparison
 */
void Graph::random_aggregate_with_weight_comparison()
{
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    for (quint32 i = 0; i < myVertexList.size(); i++)
    {
        Vertex * v = myVertexList.at(i);
        v->setWeight(v->getNumberEdge());
    }

    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;
    quint32 t = 0;
    QTime t0;
    t0.start();

    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        quint32 size = players.size();
        std::uniform_int_distribution<quint32> distribution(0,size-1);
        quint32 selected_index = distribution(generator);
        Vertex * selected = players.at(selected_index);
        //get a neighbour
        quint32 no_neighbour = selected->getNumberEdge();
        if (no_neighbour == 0) // if there is no neighbour, declare a winner
        {
            winners.append(selected);
            players.removeOne(selected);
            t++;
        }
        else // else absorb
        {

            std::uniform_int_distribution<quint32> distribution2(0,no_neighbour-1);
            quint32 selected_edge_index = distribution2(generator);
            Edge * e = selected->getEdge(selected_edge_index);
            Vertex * neighbour = selected->get_neighbour_fromEdge(selected_edge_index); //get the neighbour (not clean)
            Vertex * winner, * loser;
            quint64 selected_w = selected->getWeight(), neighbour_w = neighbour->getWeight();
            if (selected_w >= neighbour_w)
            {
                winner = selected;
                loser = neighbour;
            }
            else
            {
                winner = neighbour;
                loser = selected;
            }
            hierarchy.append(qMakePair(loser->getIndex(), winner->getIndex()));
            winner->absorb_removeEdge(e);
            players.removeOne(loser);
            t++;
        }
    }
    record_time_and_number_of_cluster(RandomAgg::I_c,t0.elapsed(),winners.size());
    centroids = winners;
    qDebug("I.c - Time elapsed: %d ms", t0.elapsed());
    large_graph_parse_result();
}

/** Type II.a - Select Neighbour With the ORIGINAL DEGREE BIAS
 * Pr(v) = u.a.r
 * Pr(u) = w(u) / w(i) forall i in adj(v)
 * @brief Graph::random_aggregate_with_neighbour_degree_bias
 */
void Graph::random_aggregate_with_neighbour_initial_degree_bias()
{
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    for (int i = 0; i < myVertexList.size(); i++)
    {
        Vertex * v = myVertexList.at(i);
        v->setWeight(v->getNumberEdge());
    }
    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;
  //  QSequentialAnimationGroup * group_anim = new QSequentialAnimationGroup;
    quint32 t = 0;
    QTime t0;
    t0.start();

    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        quint32 size = players.size();
        std::uniform_int_distribution<quint32> distribution(0,size-1);
        quint32 selected_index = distribution(generator);
        Vertex * selected = players.at(selected_index);
        //get a neighbour
        quint32 no_neighbour = selected->getNumberEdge();
        if (no_neighbour == 0) // if there is no neighbour, declare a winner
        {
            winners.append(selected);
            players.removeOne(selected);
            t++;
        }
        else // else absorb
        {
            Vertex * neighbour = selected->aggregate_get_degree_biased_neighbour();
            Edge * e = selected->getEdgeFromVertex(neighbour);
            //create the animation
            selected->absorb_removeEdge(e);
            hierarchy.append(qMakePair(neighbour->getIndex(), selected->getIndex()));
            players.removeOne(neighbour);
            t++;
        }
    }
    record_time_and_number_of_cluster(RandomAgg::II_a,t0.elapsed(),winners.size());
    centroids = winners;
    qDebug("II.a - Time elapsed: %d ms", t0.elapsed());
    large_graph_parse_result();
}

/** Rule II.a_i
 * @brief Graph::random_aggregate_with_neighbour_initial_degree_bias_with_constraint
 */
void Graph::random_aggregate_with_neighbour_initial_degree_bias_with_constraint()
{
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    for (int i = 0; i < myVertexList.size(); i++)
    {
        Vertex * v = myVertexList.at(i);
        v->setWeight(v->getNumberEdge());
    }
    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;

    int t = 0;
    QTime t0;
    t0.start();
    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        int size = players.size();
        std::uniform_int_distribution<int> distribution(0,size-1);
        int selected_index = distribution(generator);
        Vertex * selected = players.at(selected_index);
        //get a neighbour
        int no_neighbour = selected->getNumberEdge();
        if (no_neighbour == 0) // if there is no neighbour, declare a winner
        {
            winners.append(selected);
            players.removeOne(selected);
            t++;
        }
        else // else absorb
        {
            Vertex * neighbour = selected->aggregate_get_degree_biased_neighbour();
            Edge * e = selected->getEdgeFromVertex(neighbour);
            Vertex * winner, * loser;
            if (selected->getWeight() >= neighbour->getWeight())
            {
                winner = selected;
                loser = neighbour;
            }
            else
            {
                winner = neighbour;
                loser = selected;
            }
            winner->absorb_removeEdge(e);
            hierarchy.append(qMakePair(loser->getIndex(), winner->getIndex()));
            players.removeOne(loser);
            t++;
        }
    }
    record_time_and_number_of_cluster(RandomAgg::II_a_i,t0.elapsed(),winners.size());
    centroids = winners;
    qDebug("II.a(i) - Time elapsed: %d ms", t0.elapsed());
    large_graph_parse_result();
}

/** Type II.b - Select Neighbour With the CURRENT Degree Bias
 * Pr(v) = u.a.r
 * Pr(u) = d(u) / sum d(i) forall i in adj(v)
 * u -> v
 * @brief Graph::random_aggregate_with_neighbour_CURRENT_degree_bias
 */
void Graph::random_aggregate_with_neighbour_CURRENT_degree_bias()
{
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;

    quint32 t = 0;
    QTime t0;
    t0.start();

    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        quint32 size = players.size();
        std::uniform_int_distribution<quint32> distribution(0,size-1);
        quint32 selected_index = distribution(generator);
        Vertex * selected = players.at(selected_index);
        //get a neighbour
        quint32 no_neighbour = selected->getNumberEdge();
        if (no_neighbour == 0) // if there is no neighbour, declare a winner
        {
            winners.append(selected);
            players.removeOne(selected);
            t++;
        }
        else // else absorb
        {
            Edge * e = selected->getDegreeProbabilisticEdge();
            Vertex * neighbour = selected->get_neighbour_fromEdge(e);
            selected->absorb_removeEdge(e);
            hierarchy.append(qMakePair(neighbour->getIndex(), selected->getIndex()));
            players.removeOne(neighbour);
            t++;
        }
    }
    record_time_and_number_of_cluster(RandomAgg::II_b,t0.elapsed(),winners.size());
    centroids = winners;
    qDebug("II.b - Time elapsed: %d ms", t0.elapsed());
    large_graph_parse_result();
}

/** Rule II.b_i
 * @brief Graph::random_aggregate_with_neighbour_CURRENT_degree_bias_with_constraint
 */
void Graph::random_aggregate_with_neighbour_CURRENT_degree_bias_with_constraint()
{
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;

    int t = 0;
    QTime t0;
    t0.start();
    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        int size = players.size();
        std::uniform_int_distribution<int> distribution(0,size-1);
        int selected_index = distribution(generator);
        Vertex * selected = players.at(selected_index);
        //get a neighbour
        int no_neighbour = selected->getNumberEdge();
        if (no_neighbour == 0) // if there is no neighbour, declare a winner
        {
            winners.append(selected);
            players.removeOne(selected);
            t++;
        }
        else // else absorb
        {
            Edge * e = selected->getDegreeProbabilisticEdge();
            Vertex * neighbour = selected->get_neighbour_fromEdge(e);
            Vertex * winner, * loser;
            if (selected->getNumberEdge() >= neighbour->getNumberEdge())
            {
                winner = selected;
                loser = neighbour;
            }
            else
            {
                winner = neighbour;
                loser = selected;
            }
            winner->absorb_removeEdge(e);
            hierarchy.append(qMakePair(loser->getIndex(), winner->getIndex()));
            players.removeOne(loser);
            t++;
        }
    }
    record_time_and_number_of_cluster(RandomAgg::II_b_i,t0.elapsed(),winners.size());
    centroids = winners;
    qDebug("II.b(i) - Time elapsed: %d ms", t0.elapsed());
    large_graph_parse_result();
}


/** Type II.c - Aggregate HIGHEST DEGREE neighbour
 * Pr(v) = u.a.r
 * Select u: arg max d(u)
 * if d(v) < d(u) ...
 * @brief Graph::random_aggregate_highest_CURRENT_degree_neighbour
 */
void Graph::random_aggregate_highest_CURRENT_degree_neighbour()
{
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;
   // QSequentialAnimationGroup * group_anim = new QSequentialAnimationGroup;
    quint32 t = 0;
    QTime t0;
    t0.start();

    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        quint32 size = players.size();
        std::uniform_int_distribution<quint32> distribution(0,size-1);
        quint32 selected_index = distribution(generator);
        Vertex * selected = players.at(selected_index);
        //get a neighbour
        quint32 no_neighbour = selected->getNumberEdge();
        if (no_neighbour == 0) // if there is no neighbour, declare a winner
        {
            winners.append(selected);
            players.removeOne(selected);
            t++;
        }
        else // else absorb
        {
            Edge * e = selected->getHighestDegreeNeighbour();
            Vertex * neighbour = selected->get_neighbour_fromEdge(e);
            quint32 dv = selected->getNumberEdge(), du = neighbour->getNumberEdge();
            Vertex * winner, * loser;
            if (dv >= du)
            {
                winner = selected;
                loser = neighbour;
            }
            else
            {
                winner = neighbour;
                loser = selected;
            }

            winner->absorb_removeEdge(e);
            hierarchy.append(qMakePair(neighbour->getIndex(), selected->getIndex()));
            players.removeOne(loser);
            t++;
        }
    }
    record_time_and_number_of_cluster(RandomAgg::II_c,t0.elapsed(),winners.size());
    centroids = winners;
    qDebug("II.c - Time elapsed: %d ms", t0.elapsed());
    large_graph_parse_result();
}


/** Type II.d - Select Min DEGREE Neighbour
 * Pr(v) = u.a.r
 * Select u: arg min d(u): u in adj(v)
 * @brief Graph::random_aggregate_with_minimum_weight_neighbour
 */
void Graph::random_aggregate_with_minimum_weight_neighbour()
{
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    for (int i = 0; i < myVertexList.size(); i++)
    {
        Vertex * v = myVertexList.at(i);
        v->setWeight(v->getNumberEdge());
    }
    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;
   // QSequentialAnimationGroup * group_anim = new QSequentialAnimationGroup;
    quint32 t = 0;
    QTime t0;
    t0.start();

    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        quint32 size = players.size();
        std::uniform_int_distribution<quint32> distribution(0,size-1);
        quint32 selected_index = distribution(generator);
        Vertex * selected = players.at(selected_index);
        //get a neighbour
        quint32 no_neighbour = selected->getNumberEdge();
        if (no_neighbour == 0) // if there is no neighbour, declare a winner
        {
            winners.append(selected);
            players.removeOne(selected);
            t++;
        }
        else // else absorb
        {
            Edge * e = selected->getSmallestCurrentDegreeNeighbour();
            Vertex * neighbour = selected->get_neighbour_fromEdge(e);
            selected->absorb_removeEdge(e);
            hierarchy.append(qMakePair(neighbour->getIndex(), selected->getIndex()));
            players.removeOne(neighbour);
            t++;
        }
    }
    record_time_and_number_of_cluster(RandomAgg::II_d,t0.elapsed(),winners.size());
    centroids = winners;
    qDebug("II.d - Time elapsed: %d ms", t0.elapsed());
    large_graph_parse_result();
}


/** Type II.e Probabilistic Minimum Degree Neighbour (Destructive, Greedy)
 * Pr(v) = d(v)/ sum d(i) forall
 * Select u: arg min u forall u in adj(v)
 * @brief Graph::random_aggregate_probabilistic_lowest_degree_neighbour_destructive
 */
void Graph::random_aggregate_probabilistic_lowest_degree_neighbour_destructive()
{
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;

    quint32 t = 0;
    QTime t0;
    t0.start();

    while(!players.empty()) //start
    {
        QList<Vertex*> ran_list;
        for (int i = 0; i < players.size(); i++)
        {
            Vertex * v = players.at(i);
            for (quint32 j = 0; j < v->getNumberEdge(); j++)
                ran_list.append(v);
        }
        if (ran_list.size() == 0)
        {
            quint32 size = players.size();
            std::uniform_int_distribution<quint32> distribution(0,size-1);
            quint32 selected_index = distribution(generator);
            Vertex * selected = players.at(selected_index);
            winners.append(selected);
            players.removeOne(selected);
        }
        else
        {
            quint64 size = ran_list.size();
            std::uniform_int_distribution<quint64> distribution(0,size-1);
            quint64 selected_index = distribution(generator);
            Vertex * selected = ran_list.at(selected_index);
            //get a neighbour
            quint64 no_neighbour = selected->getNumberEdge();
            if (no_neighbour == 0) // if there is no neighbour, declare a winner
            {
                winners.append(selected);
                players.removeOne(selected);
            }
            else // else absorb
            {
                Edge * e = selected->getSmallestCurrentDegreeNeighbour();
                Vertex * neighbour = selected->get_neighbour_fromEdge(e); //get the neighbour (not clean)
                Vertex * winner, * loser;
                winner = selected;
                loser = neighbour;
                hierarchy.append(qMakePair(winner->getIndex(), loser->getIndex()));

                winner->absorb_removeEdge(e);
                players.removeOne(loser);
            }
        }
        t++;
    }
    record_time_and_number_of_cluster(RandomAgg::II_e,t0.elapsed(),winners.size());
    centroids = winners;
    qDebug("II.e - Time elapsed: %d ms", t0.elapsed());
    large_graph_parse_result();
}


/** Type II.f - Probabilistic Min DEGREE Neighbour (RETENTIVE)
 * Pr(v) = w(v)/ sum w(i) forall i in V
 * Select u: arg min d(u): u in adj(v)
 * u -> v: w(v) += w(u)
 * @brief Graph::random_aggregate_probabilistic_candidate_with_minimum_weight_neighbour
 */
void Graph::random_aggregate_probabilistic_candidate_with_minimum_weight_neighbour()
{
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    for (int i = 0; i < myVertexList.size(); i++)
    {
        Vertex * v = myVertexList.at(i);
        v->setWeight(v->getNumberEdge());
    }
    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;

    quint32 t = 0;
    QTime t0;
    t0.start();

    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        QList<Vertex*> ran_list;
        for (quint32 i = 0 ;i < players.size(); i++)
        {
            Vertex * v = players.at(i);
            quint64 w = v->getWeight();
            for (quint64 j = 0; j < w; j++)
                ran_list.append(v);
        }
        if (ran_list.size() == 0)
        {
            quint32 size = players.size();
            std::uniform_int_distribution<quint32> distribution(0,size-1);
            quint32 selected_index = distribution(generator);
            Vertex * selected = players.at(selected_index);
            winners.append(selected);
            players.removeOne(selected);
            t++;
        }
        else
        {
            quint64 size = ran_list.size();
            std::uniform_int_distribution<quint64> distribution(0,size-1);
            quint64 selected_index = distribution(generator);
            Vertex * selected = ran_list.at(selected_index);
            //get a neighbour
            quint32 no_neighbour = selected->getNumberEdge();
            if (no_neighbour == 0) // if there is no neighbour, declare a winner
            {
                winners.append(selected);
                players.removeOne(selected);
                t++;
            }
            else // else absorb
            {
                Edge * e = selected->getSmallestCurrentDegreeNeighbour();
                Vertex * neighbour = selected->get_neighbour_fromEdge(e); //get the neighbour (not clean)
                Vertex * winner, * loser;
                winner = selected;
                loser = neighbour;
                hierarchy.append(qMakePair(winner->getIndex(), loser->getIndex()));
                winner->absorb_removeEdge(e);
                winner->setWeight(loser->getWeight() + winner->getWeight());
                players.removeOne(loser);
                t++;
            }
        }
    }
    record_time_and_number_of_cluster(RandomAgg::II_f,t0.elapsed(),winners.size());
    centroids = winners;
    qDebug("II.f - Time elapsed: %d ms", t0.elapsed());
    // draw_dense_graph_aggregation_result();
    // group_anim->start();
    // connect(group_anim, SIGNAL(finished()), this, SLOT(large_graph_parse_result()));
    large_graph_parse_result();
}


/** Type II.g - Deterministic Max Degree Candidate (Destructive)
 * Select candidate: v: arg max d(v)
 * Select u: arg min d(u)
 * @brief Graph::random_aggregate_greedy_max_degree
 */
void Graph::random_aggregate_greedy_max_degree()
{
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;
   // QSequentialAnimationGroup * group_anim = new QSequentialAnimationGroup;
    quint32 t = 0;
    QTime t0;
    t0.start();

    while(!players.empty()) //start
    {
        QList<Vertex*> ran_list;
        quint32 max_d = 0;
        for (int i = 0; i < players.size(); i++)
        {
            Vertex * v = players.at(i);
            quint32 dv = v->getNumberEdge();
            if (dv > max_d)
            {
                max_d = dv;
                ran_list.clear();
                ran_list.append(v);
            }
            else if (dv == max_d)
            {
                ran_list.append(v);
            }
        }

        int size = ran_list.size();
        std::uniform_int_distribution<int> distribution(0,size-1);
        int selected_index = distribution(generator);
        Vertex * selected = ran_list.at(selected_index);
        //get a neighbour
        quint32 no_neighbour = selected->getNumberEdge();
        if (no_neighbour == 0) // if there is no neighbour, declare a winner
        {
            winners.append(selected);
            players.removeOne(selected);
        }
        else // else absorb
        {
            Edge * e = selected->getSmallestCurrentDegreeNeighbour();
            Vertex * neighbour = selected->get_neighbour_fromEdge(e); //get the neighbour (not clean)
            Vertex * winner, * loser;
            winner = selected;
            loser = neighbour;
            hierarchy.append(qMakePair(winner->getIndex(), loser->getIndex()));
            winner->absorb_removeEdge(e);
            players.removeOne(loser);
        }

        t++;
    }
    record_time_and_number_of_cluster(RandomAgg::II_g,t0.elapsed(),winners.size());
    centroids = winners;
    qDebug("II.g - Time elapsed: %d ms", t0.elapsed());
    large_graph_parse_result();
}


/** Type II.h (Retentive) Greedy Max Weight (candidate selection)
 * Select Candidate: v = arg max w(v)
 * Select Neighbour: u = arg min w(u)
 * @brief Graph::random_aggregate_greedy_max_weight
 */
void Graph::random_aggregate_greedy_max_weight()
{
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    for (int i = 0; i < myVertexList.size(); i++)
    {
        Vertex * v = myVertexList.at(i);
        v->setWeight(v->getNumberEdge());
    }
    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;

    quint32 t = 0;
    QTime t0;
    t0.start();

    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        QList<Vertex*> ran_list;
        quint64 max_w = 0;
        for (int i = 0; i < players.size(); i++)
        {
            Vertex * v = players[i];
            quint64 wv = v->getWeight();
            if (wv > max_w)
            {
                max_w = wv;
                ran_list.clear();
                ran_list.append(v);
            }
            else if (wv == max_w)
            {
                ran_list.append(v);
            }
        }

        int size = ran_list.size();
        std::uniform_int_distribution<int> distribution(0,size-1);
        int selected_index = distribution(generator);
        Vertex * selected = ran_list.at(selected_index);
        //get a neighbour
        quint32 no_neighbour = selected->getNumberEdge();
        if (no_neighbour == 0) // if there is no neighbour, declare a winner
        {
            winners.append(selected);
            players.removeOne(selected);
            t++;
        }
        else // else absorb
        {
            Edge * e = selected->getSmallestCurrentWeightNeighbour();
            Vertex * neighbour = selected->get_neighbour_fromEdge(e); //get the neighbour (not clean)
            Vertex * winner, * loser;
            winner = selected;
            loser = neighbour;
            hierarchy.append(qMakePair(winner->getIndex(), loser->getIndex()));
            winner->absorb_removeEdge(e);
            winner->setWeight(loser->getWeight() + winner->getWeight());
            players.removeOne(loser);
            t++;
        }

    }
    record_time_and_number_of_cluster(RandomAgg::II_h,t0.elapsed(),winners.size());
    centroids = winners;
    qDebug("II.h - Time elapsed: %d ms", t0.elapsed());
    large_graph_parse_result();
}


/** Type III.c - Select Highest Triangles Neighbour Destructive
 * Pr(v) = u.a.r
 * Select u: arg max tri(u) forall u in adj(v)
 * if w(v) > w(u) then u -> v and vice versa
 * SELECTED VERTEX V ABSORBS THE HIGHEST-TRIANGULATED VERTEX U
 * @brief Graph::random_aggregate_with_highest_triangulated_vertex
 */
void Graph::random_aggregate_with_highest_triangulated_vertex()
{
    hierarchy.clear();
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    for (int i = 0; i < myVertexList.size(); i++)
    {
        Vertex * v = myVertexList.at(i);
        v->setWeight(v->getNumberEdge());
    }
    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;
   // QSequentialAnimationGroup * group_anim = new QSequentialAnimationGroup;
    quint32 t = 0;
    QTime t0;
    t0.start();

    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        quint32 size = players.size();
        std::uniform_int_distribution<quint32> distribution(0,size-1);
        quint32 selected_index = distribution(generator);
        Vertex * selected = players.at(selected_index);
        //get a neighbour
        quint32 no_neighbour = selected->getNumberEdge();
        if (no_neighbour == 0) // if there is no neighbour, declare a winner
        {
            winners.append(selected);
            players.removeOne(selected);
            t++;
        }
        else // else absorb
        {
            Edge * e = selected->getMostMutualVertex();
            Vertex * neighbour, * winner, * loser;
            if (e->toVertex() == selected)
                neighbour = e->fromVertex();
            else
                neighbour = e->toVertex();

            if (selected == neighbour)
            {   qDebug() << "BUG CHECK" << "SELECTED POINTER == NEIGHBOUR POINTER";
                return;
            }
            quint64 u_w = selected->getWeight(), v_w = neighbour->getWeight();
            if (u_w >= v_w)
            {
                winner = selected;
                loser = neighbour;
            }
            else
            {
                winner = neighbour;
                loser = selected;
            }
            winner->absorb_removeEdge(e);
            hierarchy.append(qMakePair(loser->getIndex(), winner->getIndex()));
            players.removeOne(loser);
            t++;
        }
    }
    record_time_and_number_of_cluster(RandomAgg::III_c,t0.elapsed(),winners.size());
    centroids = winners;
    qDebug("III.c - Time elapsed: %d ms", t0.elapsed());
    large_graph_parse_result();
}

// ---------------------------------------- AGGREGATION WHITE RETAINING VERTEX -----------------------------
/** The absorbed vertices are now retained in the graph.
 * Type III.a - Highest Triangles Neighbour
 * Pr(v) = u.a.r
 * Selet u: arg max tri(u)
 * Stationary
 * @brief Graph::random_aggregate_retain_vertex_using_triangulation_and_weight_comparison
 */
void Graph::random_aggregate_retain_vertex_using_triangulation()
{
    hierarchy.clear();
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    //initialise arrays
    QList<Vertex*> players = myVertexList;
    quint32 t = 0;
    QTime t0;
    t0.start();

    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        quint32 size = players.size();
        std::uniform_int_distribution<quint32> distribution(0,size-1);
        quint32 selected_index = distribution(generator);
        Vertex * selected = players.at(selected_index);
        Vertex * neighbour, * winner, * loser;
        if (selected->getNumberEdge() == 0)
        {
            winner = selected;
            loser = selected;
            hierarchy.append(qMakePair(loser->getIndex(), winner->getIndex()));
        }
        else
        {
            Edge * e = selected->getMostMutualVertex();
            if (e->toVertex() == selected)
                neighbour = e->fromVertex();
            else
                neighbour = e->toVertex();

            winner = neighbour;
            loser = selected;
            //create the animation
            winner->absorb_retainEdge(e);
            hierarchy.append(qMakePair(loser->getIndex(), winner->getIndex()));
        }
        players.removeOne(loser);
        t++;
    }
    record_time_and_number_of_cluster(RandomAgg::III_a,t0.elapsed(),0); // the number of cluster is only determine later on
    qDebug("III.a - Time elapsed: %d ms", t0.elapsed());
    large_parse_retain_result();
    //time is recorded first
    record_time_and_number_of_cluster(RandomAgg::III_a,0,large_result.size());
}


/** Type III.b - Probabilistic Triangle Neighbour
 * Select candidate u.a.r
 * Select neighbour with Pr = tri(u)/sum_tri(u)
 * @brief Graph::random_aggregate_retain_vertex_using_probabilistic_triangulation
 */
void Graph::random_aggregate_retain_vertex_using_probabilistic_triangulation()
{
    hierarchy.clear();
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }

    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;
    quint32 t = 0;
    QTime t0;
    t0.start();

    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        quint32 size = players.size();
        std::uniform_int_distribution<quint32> distribution(0,size-1);

        quint32 selected_index = distribution(generator);
        Vertex * selected = players.at(selected_index);
        if (selected->getNumberEdge() == 0)
            players.removeOne(selected);
        else
        {
            Edge * e = selected->getProbabilisticTriangulationCoeffVertex();
            Vertex * neighbour, * winner, * loser;
            if (e->toVertex() == selected)
                neighbour = e->fromVertex();
            else
                neighbour = e->toVertex();
            winner = neighbour;
            loser = selected;
            winner->absorb_retainEdge(e);
            hierarchy.append(qMakePair(loser->getIndex(), winner->getIndex()));
            players.removeOne(loser);
        }
        t++;
    }
    record_time_and_number_of_cluster(RandomAgg::III_b,t0.elapsed(),0); // the number of cluster is only determine later on
    qDebug("III.b - Time elapsed: %d ms", t0.elapsed());
    large_parse_retain_result();
    //time is recorded first
    record_time_and_number_of_cluster(RandomAgg::III_b,0,large_result.size());
}


/** The absorbed vertices are now retained in the graph.
 * Type III.d - Probabilistic Triangles Emphaised Cluster
 * Pr(v) = u.a.r
 * f(u) = (tri(u)*2) * (extra_w(u)/no_absorbed(u))
 * Pr(u) = f(u) / sum f(i) forall i in adj(v)
 * @brief Graph::random_aggregate_retain_vertex_using_triangulation_and_weight_comparison
 */
void Graph::random_aggregate_retain_vertex_using_triangulation_times_weight()
{
    hierarchy.clear();
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    for (int i = 0; i < myVertexList.size(); i++)
    {
        Vertex * v = myVertexList.at(i);
       // v->setWeight(v->getNumberEdge());
        v->setWeight(1);
    }
    //initialise arrays
    QList<Vertex*> players = myVertexList;
    QList<Vertex*> winners;
    quint32 t = 0;
    QTime t0;
    t0.start();

    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        quint32 size = players.size();
        std::uniform_int_distribution<quint32> distribution(0,size-1);

        quint32 selected_index = distribution(generator);
        Vertex * selected = players.at(selected_index);
        if (selected->getNumberEdge() == 0)
            players.removeOne(selected);
        else
        {
            Edge * e = selected->getProbabilisticTriangulationAndWeightVertex();
            if (e == 0)
            {
                selected->setParent(selected);
                players.removeOne(selected);
                continue;
            }
            Vertex * neighbour, * winner, * loser;
            if (e->toVertex() == selected)
                neighbour = e->fromVertex();
            else
                neighbour = e->toVertex();

            winner = neighbour;
            loser = selected;
            //create the animation
            winner->absorb_retainEdge(e);
            hierarchy.append(qMakePair(loser->getIndex(), winner->getIndex()));
            players.removeOne(loser);
        }
        t++;
    }
    record_time_and_number_of_cluster(RandomAgg::III_d,t0.elapsed(),0); // the number of cluster is only determine later on
    qDebug("III.d - Time elapsed: %d ms", t0.elapsed());
    large_parse_retain_result();
    //time is recorded first
    record_time_and_number_of_cluster(RandomAgg::III_d,0,large_result.size());

}

/** Type III.e - Highest Tri(Cluster)
 * Pr(v) = u.a.r
 * LEt C(u) be the set of vertex in u cluster:
 * Select u: arg max tri(C(u)) forall u in adj(v)
 * @brief Graph::random_aggregate_retain_vertex_using_triangulation_of_cluster
 */
void Graph::random_aggregate_retain_vertex_using_triangulation_of_cluster()
{
    hierarchy.clear();
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    for (int i = 0; i < myVertexList.size(); i++)
    {
        Vertex * v = myVertexList.at(i);
        v->setWeight(v->getNumberEdge());
    }
    //initialise arrays
    QList<Vertex*> players = myVertexList;
    quint32 t = 0;
    QTime t0;
    t0.start();

    while(!players.empty()) //start
    {
        //select a vertex uniformly at random
        quint32 size = players.size();
        std::uniform_int_distribution<quint32> distribution(0,size-1);
        quint32 selected_index = distribution(generator);
        Vertex * selected = players.at(selected_index);
        if (selected->getNumberEdge() == 0)
            players.removeOne(selected);
        else
        {
            Edge * e = selected->getHighestTriangulateCluster();
            Vertex * neighbour, * winner, * loser;
            if (e->toVertex() == selected)
                neighbour = e->fromVertex();
            else
                neighbour = e->toVertex();

            winner = neighbour;
            loser = selected;
            //create the animation
            winner->absorb_retainEdge(e);
            hierarchy.append(qMakePair(loser->getIndex(), winner->getIndex()));
            players.removeOne(loser);
        }
        t++;
    }
    record_time_and_number_of_cluster(RandomAgg::III_e,t0.elapsed(),0); // the number of cluster is only determine later on
    qDebug("III.e - Time elapsed: %d ms", t0.elapsed());
    large_parse_retain_result();
    //time is recorded first
    record_time_and_number_of_cluster(RandomAgg::III_e,0,large_result.size());

}



bool Graph::checkGraphCondition()
{
    if (myVertexList.empty())
    {
        qDebug() << "V is empty, GENERATE A GRAPH FIRST!";
        return false;
    }
    if (!graphIsReady)
    {
       // qDebug() << "Graph is DISCONNECTED!";
        return false;
    }
    else return true;
}


// ------------------------- FOR LARGE GRAPH ----------------------------------------
// ------------ TOO LAZY TO SEPERATE TO A DIFFRENT PROJECT --------------------------
/** READ LARGE GRAPH WITH GROUND TRUTH COMMUNITIES
 * PARSE AND REINDEXING SO IT FOLLOWS A CONSISTENT INDICES
 * FILES ARE FROM SNAP - STANFORD
 * @brief Graph::read_large_graph_with_ground_truth_communities
 */
void Graph::read_large_graph_with_ground_truth_communities(QString filePath)
{
    //READING ...
    //parsing, file are edges file with \t seperator
    //NOTE: Vertices are not uniquely label in increasing order
    QFile file(filePath);
    if (!file.exists())
    {
        qDebug() << "- While Initialising File Not Found";
        return;
    }
    file.open(QFile::ReadOnly | QFile::Text);
    QTextStream in(&file);
    QList<QPair<int,int> > edge;
    while (!in.atEnd())
    {
        QStringList str = in.readLine().split('\t');
        if (str.size() == 2 && !str[0].startsWith("#"))
        {
            bool ok;
            int v1 = str[0].toInt(&ok), v2 = str[1].toInt(&ok);
            if (!ok)
            {
                qDebug() << "ERROR WHEN PARSING: Data not int! Terminating";
                return;
            }
            edge.append(qMakePair(v1,v2));
        }
    }
    file.close();

    qDebug() << "HERE";
    qDebug() << "FILE CLOSED \nPARSING NOW";
    QMap<int, Vertex*> v_list;
    qDebug() << edge.size();
    for (int i = 0; i < edge.size(); i++)
    {
        QPair<int,int> p = edge[i];
        int v1 = p.first, v2 = p.second;
        if (!v_list.contains(v1))
        {
            Vertex * v = new Vertex;
            v->setIndex(v1);
            v_list.insert(v1,v);
            myVertexList.append(v);
        }
        if (!v_list.contains(v2))
        {
            Vertex * v = new Vertex;
            v->setIndex(v2);
            v_list.insert(v2,v);
            myVertexList.append(v);
        }
    }

    for (int i = 0; i < edge.size(); i++)
    {
        QPair<int,int> p = edge[i];
        int v1 = p.first, v2 = p.second;
        Vertex * from = v_list.value(v1);
        Vertex * to = v_list.value(v2);
        Edge *edge = new Edge(from, to, i);
        myEdgeList.append(edge);
    }

    graphIsReady = true;
    qDebug() << "DONE HASHING" << myVertexList.size() << myEdgeList.size();
    reindexing();
 //   reindexing_ground_truth();
}

/** Load superGraph file
 * @brief Graph::read_superGraph
 * @param filePath
 */
void Graph::read_superGraph(QString edgePath, QString summaryPath)
{
    QFile sum(summaryPath);
    sum.open(QIODevice::ReadOnly | QIODevice::Text);
    QTextStream sumin(&sum);
    sumin.readLine();
    QStringList summary = sumin.readLine().split('\t');
    quint32 total_v = summary[0].toUInt(), total_e = summary[1].toUInt();
    sum.close();

    QFile file(edgePath);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QTextStream in(&file);
    in.readLine(); //skip the first
    QList<QPair<quint32,quint32> > edge;
    while (!in.atEnd())
    {
        QStringList str = in.readLine().split('\t');
        bool ok;
        quint32 from = str[0].toUInt(&ok),
                to = str[1].toUInt(&ok);
        if (!ok)
        {
            qDebug() << "- While Rebuilding HierarchyGraphs: Vertex Index Error While Reading Edge";
            return;
        }
        edge.append(qMakePair(from,to));
    }

    for (int i = 0; i < total_v; i++)
    {
        Vertex * v = new Vertex;
        v->setIndex(i);
        myVertexList.append(v);
    }

    if ((edge.size() != total_e))
    {
        qDebug() << "- While Rebuilding Hierarchy Graphs: Number of Edges Predefined and Number Of Constructed Edge Does not match";
        return;
    }
    for (int i = 0; i < edge.size(); i++)
    {
        QPair<quint32,quint32> p = edge[i];
        Vertex * from = myVertexList.at(p.first);
        Vertex * to = myVertexList.at(p.second);
        Edge * e = new Edge(from,to,i);
        myEdgeList.append(e);
    }
}



/** REINDEXING THE SNAP GRAPH (THE PROBLEM IS THAT THE INDEX IS NOT CONTINOUS)
 *  SO A VERTEX HAS 2 INDICES: SNAP INDICIES and DUMEX INDICES
 *  ALGORITHM RUNS USING DUMEX INDICES
 *  CONTENT MATCHING USING SNAP INDICIES (FOR GROUND TRUTH)
 * @brief Graph::reindexing
 */
void Graph::reindexing()
{
    qDebug() << "Reindexing Started ...";
    qDebug() << "Writing Edge!";
    QString edgePath = "C:/Users/Dumex/Desktop/SocialNetworksCollection/SNAP_DumexTemplate/edge_file.txt";
    QString originalIndexPath = "C:/Users/Dumex/Desktop/SocialNetworksCollection/SNAP_DumexTemplate/vertex_file.txt";
    QFile file(edgePath);
    file.open(QFile::WriteOnly | QFile::Text);
    QTextStream out(&file);
    //edge are Source Target Seperated by tab \t
    //begin writing edge
    for (quint32 i = 0; i < myEdgeList.size(); i++)
    {
        Edge * e = myEdgeList.at(i);
        Vertex * from = e->fromVertex();
        Vertex * to = e->toVertex();
        quint32 dumex_v = myVertexList.indexOf(from);
        quint32 dumex_u = myVertexList.indexOf(to);
        if (dumex_v == dumex_u)
        {
            qDebug() << "Error: Self Loop Edge";
            return;
        }
        out << dumex_v << "\t" << dumex_u << endl;
    }
    file.close();
    qDebug() << "Now Writing Vertex!";
    //write the node
    //Dumex Index - Original Index seperate by tab \t
    QFile vFile(originalIndexPath);
    vFile.open(QFile::WriteOnly | QFile::Text);
    QTextStream out2(&vFile);
    for (quint32 i = 0; i < myVertexList.size(); i++)
    {
        Vertex * v = myVertexList.at(i);
        quint32 origin_index = v->getIndex();
        out2 << i << "\t" << origin_index << endl;
    }
    vFile.close();
    qDebug() << "FINISHED! Check Files and Move to Correct Location";
}

/** REINDEXING THE GROUND TRUTH
 * @brief Graph::reindexing_ground_truth
 */
void Graph::reindexing_ground_truth()
{
    read_large_ground_truth_communities();
    // REINDEXING THE GROUND TRUTH FILE
    QString truthPath = "C:/Users/Dumex/Desktop/SocialNetworksCollection/SNAP_DumexTemplate/truth_file.txt";
    QFile truthfile(truthPath);
    truthfile.open(QFile::WriteOnly | QFile::Text);
    QTextStream out(&truthfile);
    qDebug() << "Reindexing the SNAP Ground Truth";
    //community are seperate by \n
    //memebrs of community are seperated by \t
    QMap<quint32, quint32> map;
    for (quint32 i = 0; i < myVertexList.size(); i++)
        map.insert(myVertexList[i]->getIndex(), i); //snap_id  -> dumex_id
    //precheck ground_truth

    for (quint32 i = 0; i < ground_truth_communities.size(); i++)
    {
        QList<quint32> c = ground_truth_communities[i]; //SNAP index
        for (quint32 j = 0; j < c.size(); j++)
        {
            //find the appropriate dumex index
            quint32 snap_id = c[j];
            if (map.contains(snap_id))
            {
                out << map.value(snap_id) << "\t";
            }
            else
            {
                qDebug() << "KEY NOT FOUND; TERMINATING!";
                return;
            }
        }
        out << endl;
    }
    truthfile.close();
    qDebug() << "DONE!";
}


void Graph::read_large_ground_truth_communities()
{
    qDebug() << "PARSING GROUND TRUTH COMMUNITIES";
    ground_truth_communities.clear();
    QString filePath;
    QFile file(filePath);
    file.open(QFile::ReadOnly | QFile::Text);
    QTextStream in(&file);
    quint32 max = 0;
    while (!in.atEnd())
    {
        QStringList str = in.readLine().split('\n'); //a community
        QStringList sub_str = str[0].split('\t'); //community split by '\t'
        QList<quint32> community;
        for (quint32 i = 0; i < sub_str.size(); i++)
        {
            bool ok;
            quint32 indx = sub_str[i].toUInt(&ok); //get vertex index
            if (ok)
            {
                community.append(indx);
                if (indx > max)
                    max = indx;
            }
        }
        ground_truth_communities.append(community);
    }
    //readjust if index does not start from 0
    if (max > myVertexList.size())
        qDebug() << "Did you load the SNAP or DUMEX file?";
    //check sum
    quint32 n = 0;
    for (int i = 0; i < ground_truth_communities.size(); i++)
        n+=ground_truth_communities[i].size();

    if (n == myVertexList.size())
    {
        qDebug() << "OK!";
    }
    qDebug() << "FINISHED! Number of Comm: " << ground_truth_communities.size();
    file.close();
}

/** Exclude Vertices Which Are Not On Ground Truth
 * @brief Graph::remove_excluded_vertices_from_ground_truth
 */
void Graph::remove_excluded_vertices_from_ground_truth()
{
    qDebug() << "- Adding Vertices Which Are Not On Ground Truth To Excluded List ...";
    QSet<quint32> on_truth;
    for (int i = 0; i < ground_truth_communities.size(); i++)
    {
        QList<quint32> c = ground_truth_communities.at(i);
        for (int j = 0; j < c.size(); j++)
        {
            quint32 id = c.at(j);
            if (!on_truth.contains(id))
                on_truth.insert(id);
        }
    }
    for (int i = 0; i < myVertexList.size(); i++)
    {
        quint32 id = myVertexList.at(i)->getIndex();
        if (!on_truth.contains(id))
            large_excluded.insert(id);
    }
    qDebug() << "- Number of Excluded: " << large_excluded.size();
}


/** Require Clarification
 * We Process The OverLap vertices
 * for now, for overlap vertices, retain each vertex in the largest community
 * @brief Graph::large_process_overlap
 */
void Graph::large_process_overlap()
{
    QMap<quint32,quint32> map;
    for (int i = 0; i < ground_truth_communities.size(); i++)
    {
        QList<quint32> c = ground_truth_communities[i];
        for(int j = 0; j < c.size(); j++)
        {
            quint32 id = c[j];
            if (!map.contains(id))
                map.insert(id,i);
            else
                map.insertMulti(id, i);
        }
    }
    QList<quint32> keys = map.keys();
    for (int k = 0; k < keys.size(); k++)
    {
        quint32 id = keys[k];
        QList<quint32> comms = map.values(id);
        if (comms.size() > 1) //belong to more than 1 community
        {
            quint32 largest_comm_size = 0, chosen_comm = 0;
            for (int i = 0; i < comms.size(); i++)
            {
                quint32 cid = comms[i];
                quint32 c_size = ground_truth_communities[cid].size();
                if (c_size > largest_comm_size)
                {
                    largest_comm_size = c_size;
                    chosen_comm = i;
                }
            }

            for(int i = 0; i < comms.size(); i++)
            {
                if (i != chosen_comm)
                {
                    ground_truth_communities[comms[i]].removeOne(id);
                }
            }
        }
    }
    //final check
    quint32 n = 0;
    for (int i = 0; i < ground_truth_communities.size(); i++)
        n += ground_truth_communities[i].size();
    qDebug() << n;
}

/** Process Ground Truth by Seperating the Overlapped Vertices and the Non-Overlapped Vertices
 * @brief Graph::large_process_overlap_by_seperate_intersection
 */
void Graph::large_process_overlap_by_seperate_intersection()
{
    qDebug() << "- Processing Overlap by Seperating Intersections ...";
    QMap<quint32,quint32> map;
    for (int i = 0; i < ground_truth_communities.size(); i++)
    {
        QList<quint32> c = ground_truth_communities[i];
        for(int j = 0; j < c.size(); j++)
        {
            quint32 id = c[j];
            if (!map.contains(id))
                map.insert(id,i);
            else
                map.insertMulti(id, i);
        }
    }

    QList<quint32> keys = map.keys();
    qDebug() << "- Processing Overlap: Number of Vertices: " << keys.size();
    for (int k = 0; k < keys.size(); k++)
    {
        quint32 id = keys[k];
        QList<quint32> comms = map.values(id);
        if (comms.size() > 1) //belong to more than 1 community
        {
            for (int i = 0; i < comms.size(); i++)
            {
                quint32 cid = comms[i];
                ground_truth_communities[cid].removeOne(id);
                large_excluded.insert(id);
            }
        }
    }

    quint32 no_v = 0;
    for (int i = 0; i < ground_truth_communities.size(); i++)
        no_v += ground_truth_communities[i].size();

    qDebug() << QString("- Seperating Overlapped Vertices Sucessful:\t- Number of Non-Overlapped V: %1").arg(no_v);
}

/** Preprocess Ground-Truth Communities
 * Let X be the communities: X = {X1,X2,..}
 * for every pair of communities
 * if X1 \cap X2 >= 1/2 of X1 then merge
 * @brief Graph::large_process_overlap_by_merge_intersection
 */
void Graph::large_process_overlap_by_merge_intersection()
{

}


/** RELOAD SNAP FILE IN DUMEX FORMAT
 * @brief Graph::read_DUMEX_input
 */

void Graph::read_DUMEX_input(QString dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists())
    {
        qDebug() << "DIR NOT EXISTS! Terminating ...";
        return;
    }
    globalDirPath = dirPath;
    QStringList filters;
    filters << "*.txt";
    QFileInfoList file = dir.entryInfoList(filters);
    QString v_file, e_file, t_file;
    for (int i = 0; i < file.size(); i++)
    {
        QFileInfo f = file.at(i);
        QString name = f.fileName();
        if (name.contains("edge"))
            e_file = f.absoluteFilePath();
        else if (name.contains("vertex"))
            v_file = f.absoluteFilePath();
        else if (name.contains("truth"))
            t_file = f.absoluteFilePath();
        else
            qDebug() << "ERROR READING FILES: File not Found!";
    }

    //reload original vertices
    //Parsing
    QFile efile(e_file), vfile(v_file), tfile(t_file);
    if (!efile.exists() || !vfile.exists() || !tfile.exists())
    {
        qDebug() << "FILE NOT FOUND! Recheck! Terminating ...";
        return;
    }
    //else
    vfile.open(QFile::ReadOnly | QFile::Text);
    QTextStream vin(&vfile);
    QStringList str = vin.readLine().split('\t');
    bool load;
    global_v = str[0].toUInt(&load);
    global_e = str[1].toUInt(&load);
    if (!load)
    {
        qDebug() << "ERROR LOADING V FILE";
        return;
    }
    vfile.close();
    //READ E FILE
    efile.open(QFile::ReadOnly | QFile::Text);
    QTextStream ein(&efile);
    QList<QPair<quint32,quint32> > edge;
    while (!ein.atEnd())
    {
        QStringList str = ein.readLine().split('\t');
        bool ok;
        quint32 v1 = str[0].toUInt(&ok), v2 = str[1].toUInt(&ok);
        if (ok)
        {
            edge.append(qMakePair(v1,v2));
        }
    }
    efile.close();

    qDebug() << "FINISHED LOADING DUMEX_TEMPLATE GRAPH!";
    qDebug() << "V:" << global_v << "; E:" << global_e;
    /*
    //reload original vertices
    vfile.open(QFile::ReadOnly | QFile::Text);
    QTextStream vin(&vfile);
    QList<int> origin_v;
    for (int i = 0; i < boost::num_vertices(g); i++)
        origin_v.append(-1);

    while (!vin.atEnd())
    {
        QStringList str = vin.readLine().split('\t');
        if (str.size() == 2 && !str.startsWith("#"))
        {
            int dumex_index = str[0].toInt(),
                SNAP_index = str[1].toInt();
            origin_v.replace(dumex_index, SNAP_index);
        }
    }
    vfile.close();
    //checking
    for (int i = 0; i < origin_v.size(); i++)
    {
        if (origin_v[i] == -1)
        {
            qDebug() << "ERROR: INDEX MISSING";
            return;
        }
    }
    qDebug() << "FINISHED RELOAD SNAP INDICES!";
    */
    //create Vertex and Edge object DECAPREATED
    for (quint32 i = 0; i < global_v; i++)
    {
        Vertex * v = new Vertex;
        v->setIndex(i);
        myVertexList.append(v);
    }

    for (quint32 i = 0; i < global_e; i++)
    {
        QPair<quint32,quint32> p = edge[i];
        quint32 from = p.first, to = p.second;
        Vertex * vfrom = myVertexList.at(from);
        Vertex * vto = myVertexList.at(to);
        Edge * e = new Edge(vfrom,vto,i);
        myEdgeList.append(e);
    }
    //check sum
    bool fit = false;
    if (myVertexList.size() == global_v && myEdgeList.size() == global_e)
        fit = true;
    qDebug() << "Check Sum" << fit;
    if(fit)
    {
        graphIsReady = true;
        qDebug() << "PREQUISITE: OK! READING TRUTH FILES";
        ground_truth_communities.clear();
        QFile tfile(t_file);
        tfile.open(QFile::ReadOnly | QFile::Text);
        QTextStream tin(&tfile);
        quint32 max = 0;
        while (!tin.atEnd())
        {
            QStringList str = tin.readLine().split('\n'); //a community
            QStringList sub_str = str[0].split('\t'); //community split by '\t'
            QList<quint32> community;
            for (int i = 0; i < sub_str.size(); i++)
            {
                bool ok;
                quint32 indx = sub_str[i].toUInt(&ok); //get vertex index
                if (ok)
                {
                    community.append(indx);
                    if (indx > max)
                        max = indx;
                }
            }
            ground_truth_communities.append(community);
        }
        //readjust if index does not start from 0
        if (max > myVertexList.size())
            qDebug() << "Did you load the SNAP or DUMEX file?";
        //check sum
        quint32 n = 0;
        for (int i = 0; i < ground_truth_communities.size(); i++)
            n+=ground_truth_communities[i].size();

        if (n == myVertexList.size())
        {
            qDebug() << "OK!";
        }
        qDebug() << "FINISHED! Number of Comm: " << ground_truth_communities.size();
        tfile.close();
        //check,
        QSet<quint32> clustered;

        for (int i = 0; i < ground_truth_communities.size(); i++)
        {
            QList<quint32> c = ground_truth_communities[i];
            for (int j = 0; j < c.size(); j++)
            {
                if (!clustered.contains(c[j]))
                    clustered.insert(c[j]);
                if (c[j] > myVertexList.size())
                    qDebug() << "ERROR: index > size";
            }
        }
        for (quint32 i = 0; i < myVertexList.size(); i++)
        {
            if (!clustered.contains(i))
                large_excluded.insert(i);
        }
        //reset index
        for (quint32 i = 0; i < myVertexList.size(); i++)
        {
            myVertexList[i]->setIndex(i);
        }
        qDebug() << "Number of Vertex Excluded From SNAP Community:" << large_excluded.size();
        qDebug() << "Removing Overlap (By Assigning each vertex to the largest)";
        large_process_overlap();
        graphIsReady = true;
    }
    else
    {
        qDebug() << "Check Sum FAILS!";
    }
}

/** PARSE AGGREGATING RESULT FROM LARGE GRAPH
 * @brief Graph::large_graph_parse_result
 */
void Graph::large_graph_parse_result()
{
    qDebug() << "PARSING RESULT";
    if (centroids.empty())
    {
        qDebug() << "WINNER SET IS EMPTY";
        return;
    }
    QList<QList<quint32> > C;
    //qDebug() << "C: " << centroids.size();
    for (int i = 0; i < centroids.size(); i++)
    {
        QList<quint32> c;
        Vertex * v = centroids.at(i);
        quint32 v_i = v->getIndex();
        if (!large_excluded.contains(v_i))
            c.append(v_i);
        QList<Vertex*> absorbed = v->getAbsorbedList();
        for (int j = 0; j < absorbed.size(); j++)
        {
            quint32 u_i = absorbed[j]->getIndex();
            if (!large_excluded.contains(u_i))
                c.append(u_i); //get the SNAP index
        }
        //exclude vertices that is not included in SNAP
        if (c.size() > 0)
            C.append(c);
    }
    /** Prepare data for indicies matching
      */
    large_result = C;
    C.clear();
    /*
    qDebug() << "- Number of Clusters: " << large_result.size();
    this->clear_edge();
    if (ground_truth_communities.empty()) //for non ground truth parsing
    {
        qDebug() << "GROUND TRUTH COMMUNITIES HAS NOT BEEN LOADED OR GRAPH HAS NOT BEEN CLUSTERED";
        qDebug() << "Only Modularity Can Be Calculated:";
        if (no_run == 0)
            LARGE_reload_edges();
        else
            LARGE_reload_superEdges();

        qDebug() << "Q: " << LARGE_compute_modularity();
    }
    else
    {   //count number of unique elements in RESULT and in Ground_truth
        quint32 uniq = count_unique_element();
        if (uniq > 0)
        {
            LARGE_compute_cluster_matching(uniq);
            qDebug() << "- Calculating Q: ";
            LARGE_reload_edges();
            double Q = LARGE_compute_modularity();
            qDebug() << "Q:" << Q;
        }
        else
            qDebug() << "ERROR: NUMBER OF UNIQUE ELEMTNS IN RESULT DIFF IN GROUND TRUTH (AFTER CHECKING EXCLUDED)";
    }*/
    graphIsReady = false;
}

/** Parse result of retain
 * @brief Graph::large_parse_retain_result
 */
void Graph::large_parse_retain_result()
{
    typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS> NormalGraph;
    NormalGraph g;
    for(int i = 0; i < myVertexList.size(); i++)
        boost::add_vertex(g);

    for (int i = 0; i < hierarchy.size(); i++)
    {
        QPair<quint32,quint32> p = hierarchy.at(i);
        boost::add_edge(p.first, p.second, g);
    }

    std::vector<quint32> component(boost::num_vertices(g));
    quint32 num = boost::connected_components(g, &component[0]);
    qDebug() << "Number of Clusters:" << num;
    //compute indices
    QList<QList<quint32> > clusters;
    for (int i = 0 ; i < num; i++)
    {
        QList<quint32> c;
        clusters.append(c);
    }

    for (int i = 0; i < component.size(); i++)
    {
        if (!large_excluded.contains(i))
        {
            QList<quint32> c = clusters[component[i]];
            c.append(i);
            clusters.replace(component[i],c);
        }
    }
    large_result = clusters;

    clusters.clear();
//    print_result_stats();
    if (ground_truth_communities.empty()) //for non ground truth parsing
    {
        qDebug() << "GROUND TRUTH COMMUNITIES HAS NOT BEEN LOADED OR GRAPH HAS NOT BEEN CLUSTERED";
        qDebug() << "Only Modularity Can Be Calculated:";
        qDebug() << "Q: " << LARGE_compute_modularity();
    }
    else
    {   //count number of unique elements in RESULT and in Ground_truth
        quint32 uniq = count_unique_element();
        if (uniq > 0)
        {
            LARGE_compute_cluster_matching(uniq);
            LARGE_compute_modularity();
        }
        else
            qDebug() << "ERROR: NUMBER OF UNIQUE ELEMTNS IN RESULT DIFF IN GROUND TRUTH (AFTER CHECKING EXCLUDED)";
    }
    graphIsReady = false;
}


void Graph::print_result_stats()
{
    qDebug() << "- Writing Log ...";
    QString logPath = globalDirPath + "/log.txt";
    QFile outFile(logPath);
    outFile.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream out(&outFile);
    //print size of clusters
    quint32 sm = 0, isolated = 0;
    out << "****************** 1 RUN ***********************" << endl;
    out << "Cluster#\tSize" << endl;
    for (int i = 0; i < large_result.size(); i++)
    {
        quint32 size = large_result[i].size();
        if (size == 1)
            isolated++;
        else if (size > 1 && size <= 3)
            sm++;
        else
            out << i <<'\t' << large_result[i].size() << endl;
    }
    out << "********************************" << endl;
    out << "Summary: Total C:" << large_result.size() << endl
        << "Small Size C ( < 3): " << sm << "; Isolated (== 1): " << isolated;
    outFile.close();
    qDebug() << " - DONE!!!";
}

/** CREATE THIS LOG FILE
 * @brief Graph::create_time_and_number_of_cluster_file
 * the matrix format is:
 *      Type \tab average_t \tab average_c
 *      I.a  \tab   xx      \tab    yy
 *      ...
 */
void Graph::create_time_and_number_of_cluster_file()
{
    QString filePath = globalDirPath + "/t_and_c_log.txt";
    QFile file(filePath);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << "Type \t average_t(msec) \t average_c \n";
    QStringList types;
    types << "I.a"<< "I.b"<< "I.c"
          << "II.a"<< "II.a.i"<< "II.b"<< "II.b.i"<< "II.c"<< "II.d"<< "II.e"<< "II.f"<< "II.g"<< "II.h"
          << "III.a"<< "III.b"<< "III.c"<< "III.d"<< "III.e"
          << "GN_Clustering"<< "CNM_Clustering"
          << "R1a"<<"I_x"<< "RFD";
    for(int i = 0; i < types.size(); i++)
    {
        out << types[i] << '\t' << QString::number(0) << '\t' << QString::number(0) << '\n';
    }
    file.close();
}

/** RECORD TIME EXECUTED AND NUMBER OF CLUSTER
 * @brief Graph::record_time_and_number_of_cluster
 */
void Graph::record_time_and_number_of_cluster(int AlgorithmType, int t, int c)
{
    QString fileName = "t_and_c_log.txt";
    if (!locate_file_in_dir(fileName))
    {
        create_time_and_number_of_cluster_file();
        locate_file_in_dir(fileName);
    }

    QFile outFile(fileName);
    outFile.open(QIODevice::ReadWrite | QIODevice::Text);
    QTextStream in(&outFile);
    int id = AlgorithmType + 1; //first line is skipped
    QStringList types = in.readAll().split('\n');
    QString record = types.at(id); //whole record
    QStringList t_and_c = record.split('\t'); //get a type record
    //start rewriting
    int prev_t = t_and_c.at(1).toInt(),
        prev_c = t_and_c.at(2).toInt();
    int avg_t = 0, avg_c = 0;
    if (t == 0) avg_t = prev_t;
    else avg_t = (prev_t +  t)/2;
    if (c == 0) avg_c = prev_c;
    else avg_c = (prev_c + c)/2;
    QString new_record = QString(t_and_c.at(0)+ '\t'
               + QString::number(avg_t) + '\t'
               + QString::number(avg_c));
    types.replace(id, new_record);
    //rewrite
    outFile.resize(0);
    for(int i = 0; i < types.size(); i++)
        in << types.at(i) << '\n';

    outFile.close();
}

/** Save a specific community
 * Sample GML
 * @brief Graph::print_single_community
 */
void Graph::print_single_community_inGraphML(const QList<quint32> &com, int k)
{
    qDebug() << "- Writing Community File as GML To working DIR ...";
    QString dirPath(globalDirPath + "/GraphML");
    QDir dir(dirPath);
    if (!dir.exists())
        QDir().mkdir(dirPath);
    QString logPath = dirPath + "/specific_community" + QString::number(k)+".graphml";
    QFile outFile(logPath);
    outFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&outFile);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl
        <<   "<graphml>" << endl;
    //specify attribute for vertices
    //<key id="d0" for="node" attr.name="color" attr.type="string">
    //<default>yellow</default>
    //</key>
    out << "<key id=\"c\" for=\"node\" attr.name=\"Cluster\" attr.type=\"int\">" << endl;
    out << "<default>0</default>" << endl;
    out << "</key>" << endl;
    out << "<graph id=\"G\" edgedefault=\"undirected\">" << endl;
    //begin writing vertices OR in this case node
    for (int i = 0; i < com.size(); i++)
    {
        Vertex * v = myVertexList.at(com[i]);
        QString node = QString("<node id=\"%1\">").arg(v->getIndex());
        out << node << endl;
        QString data = QString("<data key=\"c\">%1</data>").arg(1);
        out << data << endl;
        out << "</node>" << endl;
    }
    // now writing edge

    quint32 count_e = 0;
    QSet<QPair<quint32,quint32> > unique_e;
    for (int i = 0; i < com.size(); i++)
    {
        quint32 id = com[i];
        Vertex * v = myVertexList.at(id);
        QList<Edge*> e = v->getAllEdge();
        for (int j = 0; j < e.size(); j++)
        {
            QPair<quint32,quint32> p = qMakePair(e[j]->fromVertex()->getIndex()
                                                 , e[j]->toVertex()->getIndex());
            QPair<quint32,quint32> r_p = qMakePair(p.second,p.first);
            if (!unique_e.contains(p) && !unique_e.contains(r_p))
            {
                unique_e.insert(p);
                unique_e.insert(r_p);
                QString eout = QString("<edge id=\"%1\" source=\"%2\" target=\"%3\"/>")
                        .arg(e[j]->getIndex()).arg(p.first).arg(p.second);
                out << eout << endl;
                count_e++;
            }
            else
            {
                if (!unique_e.contains(p))
                    unique_e.insert(p);
                if (!unique_e.contains(r_p))
                    unique_e.insert(r_p);
            }

        }
    }
    out << "</graph>\n</graphml>";
    outFile.close();
    qDebug() << "Number of Vertices: " << com.size();
    qDebug() << "Number of Edges: " << count_e;
    qDebug() << " - DONE!!!";
}

/** Print Multiple
 * @brief Graph::print_multiple_communities_inGraphML
 * @param list: list of community indices, which will be printed together, thus forming a large graph
 */
void Graph::print_multiple_communities_inGraphML(const QList<quint32> &list)
{
    qDebug() << "- Writing Community File as GML To working DIR ...";
    QString dirPath(globalDirPath + "/GraphML/");
    QDir dir(dirPath);
    if (!dir.exists())
        QDir().mkdir(dirPath);
    QString logPath = dirPath + "/multiple_communities" + QString::number(list.size())+".graphml";
    QFile outFile(logPath);
    outFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&outFile);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl
        <<   "<graphml>" << endl;
    //specify attribute for vertices
    //<key id="d0" for="node" attr.name="color" attr.type="string">
    //<default>yellow</default>
    //</key>
    out << "<key id=\"c\" for=\"node\" attr.name=\"Cluster\" attr.type=\"int\">" << endl;
    out << "<default>0</default>" << endl;
    out << "</key>" << endl;
    out << "<graph id=\"G\" edgedefault=\"undirected\">" << endl;
    //begin writing vertices OR in this case node
    for (int i = 0; i < list.size(); i++ )
    {
        QList<quint32> com = large_result.at(list[i]);
        for (int j = 0; j < com.size(); j++)
        {
            Vertex * v = myVertexList.at(com[j]);
            QString node = QString("<node id=\"%1\">").arg(v->getIndex());
            out << node << endl;
            QString data = QString("<data key=\"c\">%1</data>").arg(i);
            out << data << endl;
            out << "</node>" << endl;
        }
    }
    // now writing edge

    QSet<quint32> unique_e;
    for (int i = 0; i < list.size(); i++)
    {
        QList<quint32> com = large_result.at(list[i]);
        for(int j = 0; j < com.size(); j++)
        {
            quint32 id = com[j];
            Vertex * v = myVertexList.at(id);
            QList<Edge*> e_list = v->getAllEdge();
            for (int k = 0;k < e_list.size();k++)
            {
                Edge * e = e_list[k];
                quint32 e_id = e->getIndex();
                if (!unique_e.contains(e_id))
                {
                    unique_e.insert(e_id);
                    QString eout = QString("<edge id=\"%1\" source=\"%2\" target=\"%3\"/>")
                            .arg(e_id).arg(e->fromVertex()->getIndex()).arg(e->toVertex()->getIndex());
                    out << eout << endl;
                }
            }
        }
    }
    out << "</graph>\n</graphml>";
    outFile.close();
    qDebug() << " - DONE!!!";
}

/** Override
 * @brief Graph::print_multiple_communities_inGraphML
 * @param str
 */
void Graph::print_multiple_communities_inGraphML(QString str)
{
    qDebug() << "- Writing Community File as GML To location: " << str;
    QString logPath = str;
    QFile outFile(logPath);
    outFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&outFile);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl
        <<   "<graphml>" << endl;
    //specify attribute for vertices
    //<key id="d0" for="node" attr.name="color" attr.type="string">
    //<default>yellow</default>
    //</key>
    out << "<key id=\"c\" for=\"node\" attr.name=\"Cluster\" attr.type=\"int\">" << endl;
    out << "<default>0</default>" << endl;
    out << "</key>" << endl;
    out << "<graph id=\"G\" edgedefault=\"undirected\">" << endl;
    //begin writing vertices OR in this case node
    for (int i = 0; i < large_result.size(); i++ )
    {
        QList<quint32> com = large_result.at(i);
        for (int j = 0; j < com.size(); j++)
        {
            Vertex * v = myVertexList.at(com[j]);
            QString node = QString("<node id=\"%1\">").arg(v->getIndex());
            out << node << endl;
            QString data = QString("<data key=\"c\">%1</data>").arg(i);
            out << data << endl;
            out << "</node>" << endl;
        }
    }
    // now writing edge

    QSet<quint32> unique_e;
    for (int i = 0; i < large_result.size(); i++)
    {
        QList<quint32> com = large_result.at(i);
        for(int j = 0; j < com.size(); j++)
        {
            quint32 id = com[j];
            Vertex * v = myVertexList.at(id);
            QList<Edge*> e_list = v->getAllEdge();
            for (int k = 0;k < e_list.size();k++)
            {
                Edge * e = e_list[k];
                quint32 e_id = e->getIndex();
                if (!unique_e.contains(e_id))
                {
                    unique_e.insert(e_id);
                    QString eout = QString("<edge id=\"%1\" source=\"%2\" target=\"%3\"/>")
                            .arg(e_id).arg(e->fromVertex()->getIndex()).arg(e->toVertex()->getIndex());
                    out << eout << endl;
                }
            }
        }
    }
    out << "</graph>\n</graphml>";
    outFile.close();
    qDebug() << " - DONE!!!";
}

/** Save a single Community with only Intra Edge
 * @brief Graph::print_single_community_with_only_intra_edge_inGraphML
 * @param list: community
 * @param k: index of community
 */
void Graph::print_single_community_with_only_intra_edge_inGraphML(int k)
{
    qDebug() << "- Writing Community File as GML To working DIR ...";
    QString dirPath(globalDirPath + "/GraphML");
    QDir dir(dirPath);
    if (!dir.exists())
        QDir().mkdir(dirPath);
    QString logPath = dirPath + "/community_" + QString::number(k)+"_only_intra_edges.graphml";
    QFile outFile(logPath);
    outFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&outFile);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl
        <<   "<graphml>" << endl;
    //specify attribute for vertices
    //<key id="d0" for="node" attr.name="color" attr.type="string">
    //<default>yellow</default>
    //</key>
    out << "<key id=\"c\" for=\"node\" attr.name=\"Cluster\" attr.type=\"int\">" << endl;
    out << "<default>0</default>" << endl;
    out << "</key>" << endl;
    out << "<graph id=\"G\" edgedefault=\"undirected\">" << endl;
    //begin writing vertices OR in this case node
    QSet<quint32> unique_v;
    QList<quint32> com = large_result.at(k);
    for (int j = 0; j < com.size(); j++)
    {
        Vertex * v = myVertexList.at(com[j]);
        if (unique_v.contains(v->getIndex()))
        {
             qDebug() << " - ? Vertex Duplication in Community?";
        }
        else
            unique_v.insert(v->getIndex());
        //print
        QString node = QString("<node id=\"%1\">").arg(v->getIndex());
        out << node << endl;
        QString data = QString("<data key=\"c\">%1</data>").arg(k);
        out << data << endl;
        out << "</node>" << endl;
    }

    // now writing edge

    QSet<quint32> unique_e;
    for(int j = 0; j < com.size(); j++)
    {
        quint32 id = com[j];
        Vertex * v = myVertexList.at(id);
        QList<Edge*> e_list = v->getAllEdge();
        for (int k = 0;k < e_list.size();k++)
        {
            Edge * e = e_list[k];
            quint32 e_id = e->getIndex();
            quint32 from = e->fromVertex()->getIndex(), to = e->toVertex()->getIndex();
            if (!unique_e.contains(e_id) && (unique_v.contains(from) && unique_v.contains(to)))
            {
                unique_e.insert(e_id);
                QString eout = QString("<edge id=\"%1\" source=\"%2\" target=\"%3\"/>")
                        .arg(e_id).arg(e->fromVertex()->getIndex()).arg(e->toVertex()->getIndex());
                out << eout << endl;
            }
        }
    }

    out << "</graph>\n</graphml>";
    outFile.close();
    qDebug() << " - DONE!!!";
}




/** Print SuperV with Weight
 * @brief Graph::print_result_community_inGraphML
 */
void Graph::print_result_community__with_attributes_inGraphML()
{
    if (myVertexList.size() != large_result.size())
    {
        qDebug() << "- While Printing Results with Attributes: SuperV size does not match!";
        return;
    }
    qDebug() << "- Writing Communities File as GML To working DIR ...";
    QString dirPath(globalDirPath + "/GraphML/");
    QDir dir(dirPath);
    if (!dir.exists())
        QDir().mkdir(dirPath);
    QString logPath = dirPath + "/community_with_att_" + QString::number(no_run)+".graphml";
    QFile outFile(logPath);
    outFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&outFile);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl
        <<   "<graphml>" << endl;
    out << "<key id=\"w\" for=\"node\" attr.name=\"Weight\" attr.type=\"int\">" << endl;
    out << "<default>0</default>" << endl;
    out << "</key>" << endl;
    out << "<graph id=\"G\" edgedefault=\"undirected\">" << endl;
    //begin writing vertices OR in this case node
    for (int i = 0; i < myVertexList.size(); i++)
    {
        Vertex * v = myVertexList.at(i);
        QString node = QString("<node id=\"%1\">").arg(v->getIndex());
        out << node << endl;
        quint32 w = v->getcSize();
        QString data = QString("<data key=\"w\">%1</data>").arg(w);
        out << data << endl;
        out << "</node>" << endl;
    }
    // now writing edge

    for (int i = 0; i < myEdgeList.size(); i++)
    {
        Edge * e = myEdgeList[i];
        quint32 from = e->fromVertex()->getIndex(), to = e->toVertex()->getIndex();
        QString eout = QString("<edge id=\"%1\" source=\"%2\" target=\"%3\"/>")
                .arg(e->getIndex()).arg(from).arg(to);
        out << eout << endl;
    }
    out << "</graph>\n</graphml>";
    outFile.close();
    qDebug() << " - DONE!!!";

}



/** Print SuperV with Weight
 * Adding Colour Code or Attributes in General
 * @brief Graph::print_result_community_inGraphML
 */
void Graph::print_result_community__with_attributes_inGraphML(const QMap<quint32,QString> &colour)
{
    if (myVertexList.size() != large_result.size())
    {
        qDebug() << "- While Printing Results with Attributes: SuperV size does not match!";
        return;
    }
    qDebug() << "- Writing Communities File as GML To working DIR ...";
    QString dirPath(globalDirPath + "/GraphML");
    QDir dir(dirPath);
    if (!dir.exists())
        QDir().mkdir(dirPath);
    QString logPath = QString(dirPath + "/Gephi_community_with_W_colour_%1.graphml").arg(no_run);
    QFile outFile(logPath);
    outFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&outFile);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl
        <<   "<graphml>" << endl;
    //specify attribute for vertices
    //<key id="d0" for="node" attr.name="color" attr.type="string">
    //<default>yellow</default>
    //</key>
    out << "<key id=\"w\" for=\"node\" attr.name=\"Weight\" attr.type=\"int\">" << endl;
    out << "<default>0</default>" << endl;
    out << "</key>" << endl;
    out << "<key id=\"r\" for=\"node\" attr.name=\"red\" attr.type=\"int\">" << endl;
    out << "<default>192</default>" << endl;
    out << "</key>" << endl;
    out << "<key id=\"g\" for=\"node\" attr.name=\"green\" attr.type=\"int\">" << endl;
    out << "<default>192</default>" << endl;
    out << "</key>" << endl;
    out << "<key id=\"b\" for=\"node\" attr.name=\"blue\" attr.type=\"int\">" << endl;
    out << "<default>192</default>" << endl;
    out << "</key>" << endl;
    out << "<graph id=\"G\" edgedefault=\"undirected\">" << endl;
    //begin writing vertices OR in this case node
    for (int i = 0; i < myVertexList.size(); i++)
    {
        Vertex * v = myVertexList.at(i);
        if (colour.contains(i))
        {
            QString c = colour.value(i);
            c.remove(c.at(0));
            bool ok;
            quint32 t = c.toUInt(&ok,16);
            if(!ok)
            {
                qDebug() << "- Colour Conversion Failed!";
            }
            QList<quint32> RGB = RGB_converter(t);

            QString node = QString("<node id=\"%1\">").arg(v->getIndex());
            out << node << endl;
            quint32 w = v->getcSize();
            QString data = QString("<data key=\"w\">%1</data>").arg(w); //weight
            out << data << endl;
            QString r = QString("<data key=\"r\">%1</data>").arg(RGB[0]);
            QString g = QString("<data key=\"g\">%1</data>").arg(RGB[1]);
            QString b = QString("<data key=\"b\">%1</data>").arg(RGB[2]);
            out << r << endl;
            out << g << endl;
            out << b << endl;
            out << "</node>" << endl;
        }
        else
        {
            QString node = QString("<node id=\"%1\">").arg(v->getIndex());
            out << node << endl;
            quint32 w = v->getcSize();
            QString data = QString("<data key=\"w\">%1</data>").arg(w); //weight
            out << data << endl;
            QString r = QString("<data key=\"r\">%1</data>").arg(192);
            QString g = QString("<data key=\"g\">%1</data>").arg(192);
            QString b = QString("<data key=\"b\">%1</data>").arg(192);
            out << r << endl;
            out << g << endl;
            out << b << endl;
            out << "</node>" << endl;
        }
    }
    // now writing edge

    for (int i = 0; i < myEdgeList.size(); i++)
    {
        Edge * e = myEdgeList[i];
        quint32 from = e->fromVertex()->getIndex(), to = e->toVertex()->getIndex();
        QString eout = QString("<edge id=\"%1\" source=\"%2\" target=\"%3\"/>")
                .arg(e->getIndex()).arg(from).arg(to);
        out << eout << endl;
    }
    out << "</graph>\n</graphml>";
    outFile.close();
    qDebug() << " - DONE!!!";

}


/** Compare NUmber of Unique Element
 * @brief Graph::count_unique_element
 * @return True if match
 */
quint32 Graph::count_unique_element()
{
    QSet<quint32> res, truth;
    quint32 sum = 0;
    for (int i = 0 ; i < large_result.size(); i++)
    {
        for (int j = 0; j < large_result[i].size(); j++)
        {
            quint32 v = large_result[i][j];
            sum++;
            if (!res.contains(v))
                res.insert(v);
        }
    }
    for (int i = 0 ; i < ground_truth_communities.size(); i++)
    {
        for (int j = 0; j < ground_truth_communities[i].size(); j++)
        {
            quint32 v = ground_truth_communities[i][j];
            if (!truth.contains(v))
                truth.insert(v);
        }
    }

    if (res.size() != truth.size())
    {
        qDebug() << "Number of Element in Result:" << sum;
        qDebug() << "Number of Unique Elements in RESULT: " << res.size();
        qDebug() << "Number of Unique Elements in TRUTH: " << truth.size();
        return 0;
    }
    else
        return res.size();
}




/**
 * @brief Graph::LARGE_compute_cluster_matching
 */
void Graph::LARGE_compute_cluster_matching(quint32 n)
{
    //check sum not available since overlap
    //checking ground truth
    if (ground_truth_communities.empty())
    {
        qDebug() << "GROUND TRUTH COMMUNITIES HAS NOT BEEN LOADED OR GRAPH HAS NOT BEEN CLUSTERED";
        return;
    }
    qDebug() << "- Now Computing Pair-wise Matching Indicies ...";
    LARGE_compute_Pairwise_efficient(n);
    return;
}


/** Compare result using RAND index
 * ground truth is X = {x1, x2 ..., xr }
 * result is Y = {y1, y2, .., ys}
 * with r != s
 * then, count:
    a, the number of pairs of elements in S that are in the same set in X and in the same set in Y
    b, the number of pairs of elements in S that are in different sets in X and in different sets in Y
    c, the number of pairs of elements in S that are in the same set in X and in different sets in Y
    d, the number of pairs of elements in S that are in different sets in X and in the same set in Y
 *  R = {a+b}/{a+b+c+d} = {a+b}/{n \choose 2 }
 * a,b,c,d is computed using the contigency table
 * Let the ground truth X = {X1, X2 ..., XR}
 * Let the result be Y = {Y1, Y2 ... YC}
 * the table is then
 * /    X1 X2 ... Xr
 * Y1   n11 n22...n1r
 * Y2   n12 n22 ... n2r
 * .    .
 * .    .
 * YC   X1C X2C ....
 * in which n11 = X1 \intersect Y1
 * a, b, c, d is then have formula as in Hubert paper
 * NAIVE IMPLEMENTATION: PREFERABLY FOR SMALL COMMUNITIES SET
 * @brief Graph::compare_using_RAND_index
 * @return
 */
/** Optimise SPACE
 * @brief Graph::LARGE_compute_Pairwise_efficient
 * @param result
 * @return
 */
QList<double> Graph::LARGE_compute_Pairwise_efficient(int n)
{
    if (n == -1)    n = myVertexList.size();
    quint64 nij = 0, nij_minus = 0, nij_square = 0, nij_choose_2 = 0, n_choose_2 = 0;
    quint64 n_minus_1 = n-1, n_times_n_minus = n*n_minus_1;
    n_choose_2 = n_times_n_minus/2;

    int row = ground_truth_communities.size(), column = large_result.size();
    std::vector<quint64> ni, nj; //ni: sum row, nj: sum column
    for(size_t j = 0; j < column; j++)
        nj.push_back(0);

    for (size_t i = 0; i < row; i++)
    {
        quint64 sum_row = 0;
        QList<quint32> X = ground_truth_communities[i];
        for (int j = 0; j < column; j++)
        {
            QList<quint32> Y = large_result[j];
            uint32 entry = X.toSet().intersect(Y.toSet()).size(); //sets are assumed to be sorted
            quint64 entry_square = entry*entry,
                   entry_entryminus = entry*(entry-1);
            nij_minus += entry_entryminus; // nij(nij-1)
            nij_square += entry_square; // nij^2
            nij_choose_2 += entry_entryminus/2; //(nij choose 2) for adjust rand
            nij += entry;
            sum_row += entry;
            //
            quint64 sum_col = nj[j];
            quint64 new_sum = entry + sum_col;
            nj[j] = new_sum;
        }
        ni.push_back(sum_row);
    }

    quint64 n_square = 0,
            ni_sum = 0, //sum row
            nj_sum =0, // sum column
            ni_choose_2 = 0, //bionomial row
            nj_choose_2 = 0, //bionomial column
            ni_square = 0,  // sum each row square
            nj_square = 0; // sum each column square

    for (size_t i = 0; i < ni.size(); i++)
    {
        quint64 entry = ni[i];
        quint64 entry_square = std::pow(entry,2);
        quint64 entry_choose_2 = entry*(entry-1)/2;
        ni_square += entry_square;
        ni_choose_2 += entry_choose_2;
        ni_sum+=  entry;
    }

    for (size_t i = 0; i < nj.size(); i++)
    {
        quint64 entry = nj[i];
        quint64 entry_square = entry*entry;
        quint64 entry_choose_2 = entry*(entry-1)/2;
        nj_square += entry_square;
        nj_choose_2 += entry_choose_2;
        nj_sum+=  entry;
    }

    //check sum
    quint64 sum_i = 0, sum_j = 0;
    for (size_t i = 0; i < ni.size(); i++) sum_i += ni[i];
    for (size_t j = 0; j < nj.size(); j++) sum_j += nj[j];
    assert((sum_i == sum_j && sum_i == n) && ("FATAL ERROR WHILE CALCULATING PAIRWISE MATCHING"));
    ni.clear();
    nj.clear();

    n_square = std::pow(n,2);

    QList<quint64> param_a,param_b,param_c,param_d;
    param_a.push_back(nij_minus);
    param_b.push_back(ni_square);
    param_b.push_back(nij_square);
    param_c.push_back(nj_square);
    param_c.push_back(nij_square);
    param_d.push_back(n_square);
    param_d.push_back(nij_square);
    param_d.push_back(ni_square);
    param_d.push_back(nj_square);

    quint64 a = calA(param_a);
    quint64 d = calD(param_d);
    quint64 c = calC(param_c); // type iii: same diff
    quint64 b = calB(param_b); //type iv: diff same

    double RAND = (double) (a+d)/(a+b+c+d);
    double Jaccard = (double) a/(a+b+c);

    assert((a+b+c+d == n_choose_2) && "FATAL ERROR WHILE CALCULATING PAIRWISE MATCHING");
 //   qDebug() << a/(a+b+c);
    //
    QList<quint64> param_ARI;
    param_ARI.push_back(ni_choose_2);
    param_ARI.push_back(nj_choose_2);
    param_ARI.push_back(n_choose_2);
    param_ARI.push_back(nij_choose_2);
    double ARI = calAdRand(param_ARI);

    assert((RAND <= 1 && Jaccard <= 1 && ARI <= 1));
    printf("RAND: %f\tJaccard: %f\tARI: %f\n", RAND, Jaccard, ARI);
    QList<double> result;
    result.push_back(RAND);
    result.push_back(Jaccard);
    result.push_back(ARI);
    return result;
}


quint64 Graph::calA(QList<quint64> param)
{
    quint64 a = param[0];
    a = a/2;
  //  qDebug() << "a:" << a;
    return a;
}

quint64 Graph::calB(QList<quint64> param)
{
    quint64 ai = param[0], nij_square = param[1];
    quint64 b = (ai-nij_square)/2;
 //   qDebug() << "b:" << b;
    return b;
}

quint64 Graph::calC(QList<quint64> param)
{
    quint64 ni_square = param[0], nij_square = param[1];
    quint64 c = (ni_square - nij_square)/2;
  //  qDebug() << "c:" << c;
    return c;
}

quint64 Graph::calD(QList<quint64> param)
{
    quint64 n_square = param[0],
            nij_square = param[1],
            ni_square = param[2],
            nj_square = param[3];
    quint64 d = n_square + nij_square - ni_square - nj_square; //type ii: diff diff
    d /= 2;
 //   qDebug() << "d:" << d;
    return d;
}

double Graph::calAdRand(QList<quint64> param)
{
    quint64 ni_choose_2 = param[0],
            nj_choose_2 = param[1],
            n_choose_2 = param[2],
            nij_choose_2 = param[3];
    double nc = (double)ni_choose_2*nj_choose_2/n_choose_2;
    double nom = (double)(nij_choose_2 - nc);
    double sum = (double) (ni_choose_2 + nj_choose_2)/2;
    double denom = sum - nc;
    double ARI = nom/denom;
    return ARI;
}

/** Calculate the Clustering Coeffficient, which is the average over all v
 * Watts Algorithm
 * @brief Graph::cal_average_clustering_coefficient
 * @return
 */
double Graph::cal_average_clustering_coefficient()
{
    typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS> NormalGraph;
    NormalGraph g;
    for (int i = 0; i < myVertexList.size(); i++)
    {
        boost::add_vertex(g);
    }
    for(int i = 0;i  < myEdgeList.size(); i++)
    {
        Edge * e = myEdgeList.at(i);
        int from = e->fromVertex()->getIndex(), to = e->toVertex()->getIndex();
        boost::add_edge(from,to,g);
    }
    // Compute the clustering coefficients of each vertex in the graph
    // and the mean clustering coefficient which is returned from the
    // computation.
    typedef boost::exterior_vertex_property<NormalGraph, float> ClusteringProperty;
    typedef ClusteringProperty::container_type ClusteringContainer;
    typedef ClusteringProperty::map_type ClusteringMap;

    ClusteringContainer coefs(boost::num_vertices(g));
    ClusteringMap cm(coefs, g);
    double cc = boost::all_clustering_coefficients(g, cm);
    return cc;
}


/** Clear Log File
 * @brief Graph::clear_log
 */
void Graph::clear_log()
{
    QString file(globalDirPath + "log.txt");
    QFile log(file);
    if (!log.exists()){}
    else
        log.remove();
}

/** RANDOM FUNCTIONAL DIGRAPH - RANDOM MAPPING (BOLLOBAS)
 * The algorithm is as follows:
 * - select v u.a.r
 * - v maps to a neighbour u.a.r
 * - do for all v
 * @brief Graph::random_functional_digraph
 */
void Graph::random_functional_digraph()
{
    hierarchy.clear();
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    //initialise arrays

    quint32 t = 0;
    QTime t0;
    t0.start();

    for (int i = 0; i < myVertexList.size(); i++)
    {
        Vertex * selected = myVertexList.at(i); //get v
        //get u
        quint32 dv = selected->getNumberEdge();
        if (dv == 0)
        {
            hierarchy.append(qMakePair(selected->getIndex(), selected->getIndex()));
            selected->absorb_retainEdge(); //point to self
        }
        else
        {
            std::uniform_int_distribution<quint32> distribution2(0,dv-1);
            quint32 selected_edge_index = distribution2(generator);
            Edge * e = selected->getEdge(selected_edge_index);
            Vertex * neighbour = selected->get_neighbour_fromEdge(e); //get the neighbour (not clean)
            hierarchy.append(qMakePair(selected->getIndex(), neighbour->getIndex()));
            selected->absorb_retainEdge(e);
        }
        //points to that neighbour
    }
    record_time_and_number_of_cluster(RandomAgg::RFD,t0.elapsed(),0);
    qDebug("RFD - Time elapsed: %d ms", t0.elapsed());
    large_parse_retain_result();
    record_time_and_number_of_cluster(RandomAgg::RFD,0,large_result.size());
}

//////////////////////////// SNAP COMMUNITY ALGORITHM /////////////////////
/** Girvan and Newman Betweennes Centrality Clustering
 * @brief Graph::betweenness_centrality_clustering
 */
void Graph::betweenness_centrality_clustering()
{
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    qDebug() << "Girvan Newman Betweenness Centrality Clustering Started";
    int k = ground_truth_communities.size();
    PUNGraph TGraph = convertToSnapUnGraph();
    TVec < TCnCom > CommV;
   // double q = TSnap::_CommunityGirvanNewman(TGraph, CommV, k);
    QTime t0;
    t0.start();
    double q = TSnap::CommunityGirvanNewman(TGraph, CommV);
    record_time_and_number_of_cluster(RandomAgg::GN_Clustering,t0.elapsed(),CommV.Len());
    if (q < -0.5) {  TSnap::GetSccs(TGraph, CommV); } // if was not clustered
    QList<QList<quint32> > result;
    convertSnapCommtoMyComm(CommV, result);
    large_result = result;
    LARGE_compute_Pairwise_efficient(myVertexList.size());
    graphIsReady = false;
}

/** Clauset-Newman-Moore community detection method.
 * @brief Graph::fast_CMN
 */
void Graph::fast_CMN()
{
    if (!checkGraphCondition())
    {
        reConnectGraph();
    }
    qDebug() << "CNM Fast Clustering Started";
    int k = ground_truth_communities.size();
    PUNGraph TGraph = convertToSnapUnGraph();
    TVec < TCnCom > CommV;
    qDebug() << TGraph->GetEdges() << TGraph->GetNodes();
    //start time
    QTime t0;
    t0.start();
    TSnap::CommunityCNM(TGraph, CommV);
    record_time_and_number_of_cluster(RandomAgg::CNM_Clustering,t0.elapsed(),CommV.Len());
    //parse result
    QList<QList<quint32> > result;
    convertSnapCommtoMyComm(CommV, result);
    large_result = result;
    LARGE_compute_Pairwise_efficient(myVertexList.size());
    graphIsReady = false;
}
//////////////////////////// END OF SNAP COMMUNITY ALGORITHM /////////////////////

/** Reset to prepare for next run
 * @brief Graph::LARGE_reset
 */
void Graph::LARGE_reset()
{
    for (int i = 0; i < myVertexList.size(); i++)
    {
        myVertexList.at(i)->resetClusterRelevant();
        myVertexList.at(i)->removeAll();
    }
    //myEdgeList.clear();
    clear_edge();
    hierarchy.clear();
    centroids.clear();
    large_result.clear();
    graphIsReady = false;
}

/** Reload
 * @brief Graph::LARGE_reload
 */
bool Graph::LARGE_reload()
{
    qDebug() << "RELOADING";
    LARGE_reset();
    if (globalDirPath.size() == 0)
    {
        qDebug() << "GLOBAL DIR PATH HAS NOT BEEN SET!";
        return false;
    }
    //read edge only
    myEdgeList.clear();
    if (no_run == 0 )
        LARGE_reload_edges();
    else
    {
        qDebug() << "- ** Reaggregation Detected! Following are results for super graph...";
        LARGE_reload_superEdges();
    }

    graphIsReady = true;
    return graphIsReady;
}

/** RELOAD EDGES ONLY
 * This is good enough
 * @brief Graph::LARGE_reload_edges
 */
void Graph::LARGE_reload_edges()
{
    qDebug() << "- Reloading Edges ...";
    QDir dir(globalDirPath);
    QStringList filters;
    filters << "*.txt";
    QFileInfoList file = dir.entryInfoList(filters);
    QString e_file;
    for (int i = 0; i < file.size(); i++)
    {
        QFileInfo f = file.at(i);
        QString name = f.fileName();
        if (name.contains("edge"))
            e_file = f.absoluteFilePath();
    }

    //Parsing
    QFile efile(e_file);
    if (!efile.exists())
    {
        qDebug() << "FILE NOT FOUND! Recheck! Terminating ...";
        return;
    }
    //else
    efile.open(QFile::ReadOnly | QFile::Text);
    QTextStream ein(&efile);
    QList<QPair<quint32,quint32> > edge;
    ein.readLine(); //skip first line
    while (!ein.atEnd())
    {
        QStringList str = ein.readLine().split('\t');
        if(str[0].startsWith("#")) continue;
        bool ok;
        quint32 v1 = str[0].toUInt(&ok), v2 = str[1].toUInt(&ok);
        if (ok)
        {
            edge.append(qMakePair(v1,v2));
        }
    }
    efile.close();
    //reload original vertices
    //create Vertex and Edge object
    qDebug() << "- Now Loading Edges ...";
    for (int i = 0; i < edge.size(); i++)
    {
        QPair<quint32,quint32> p = edge[i];
        quint32 from = p.first, to = p.second;
        Vertex * vfrom = myVertexList.at(from);
        Vertex * vto = myVertexList.at(to);
        Edge * e = new Edge(vfrom,vto,i);
        myEdgeList.append(e);
    }
    edge.clear();
    graphIsReady = true;
}

void Graph::LARGE_reload_superEdges()
{
    qDebug() << "- Reloading Edges ...";
    QDir dir(globalDirPath);
    QStringList filters;
    filters << "*.txt";
    QFileInfoList file = dir.entryInfoList(filters);
    QString e_file;
    for (int i = 0; i < file.size(); i++)
    {
        QFileInfo f = file.at(i);
        QString name = f.fileName();
        if (name.contains("superGraph"))
        {
            int run = get_number_from_filename(name);
            if (run == -1)
                return;
            else if (run == no_run)
            {
                e_file = f.absoluteFilePath();
            }
        }

    }

    //Parsing
    QFile efile(e_file);
    if (!efile.exists())
    {
        qDebug() << "FILE NOT FOUND! Recheck! Terminating ...";
        return;
    }
    //else
    efile.open(QFile::ReadOnly | QFile::Text);
    QTextStream ein(&efile);
    QList<QPair<quint32,quint32> > edge;
    ein.readLine(); //skip first line
    while (!ein.atEnd())
    {
        QStringList str = ein.readLine().split('\t');
        bool ok;
        quint32 v1 = str[0].toUInt(&ok), v2 = str[1].toUInt(&ok);
        if (ok)
        {
            edge.append(qMakePair(v1,v2));
        }
    }
    efile.close();
    //reload original vertices
    //create Vertex and Edge object
    qDebug() << "- Now Loading Edges ...";
    for (int i = 0; i < edge.size(); i++)
    {
        QPair<quint32,quint32> p = edge[i];
        quint32 from = p.first, to = p.second;
        Vertex * vfrom = myVertexList.at(from);
        Vertex * vto = myVertexList.at(to);
        Edge * e = new Edge(vfrom,vto,i);
        myEdgeList.append(e);
    }
    edge.clear();
}



/** Rerun
 * @brief Graph::LARGE_rerun
 */
void Graph::LARGE_rerun()
{
    int per_agg = 5;
    for (int i = 50; i < 17*per_agg; i++)
    {
        int agg = i/per_agg;
        qDebug() << "********* NEW RUN START BELOWS ******** ";
        if (agg == 0){}
        else if (agg == 1)
        {
            qDebug() << "Type I.a:";
            random_aggregate();
        }
        else if (agg == 2)
        {
            qDebug() << "Type I.b:";
            random_aggregate_with_degree_comparison();
        }
        else if (agg == 3)
        {
            qDebug() << "Type I.c:";
            random_aggregate_with_weight_comparison();
        }
        else if (agg == 4)
        {
             qDebug() << "Type II.a:";
            random_aggregate_with_neighbour_initial_degree_bias();
        }
        else if (agg == 5)
        {
            qDebug() << "Type II.b:";
            random_aggregate_with_neighbour_CURRENT_degree_bias();
        }
        else if (agg == 6)
        {
            qDebug() << "Type II.c:";
            random_aggregate_highest_CURRENT_degree_neighbour();
        }
        else if (agg == 7)
        {
            qDebug() << "Type II.d:";
            random_aggregate_with_minimum_weight_neighbour();
        }
        else if (agg == 8)
        {
            qDebug() << "Type II.e:";
            random_aggregate_probabilistic_lowest_degree_neighbour_destructive();
        }
        else if (agg == 9)
        {
            qDebug() << "Type II.f:";
            random_aggregate_probabilistic_candidate_with_minimum_weight_neighbour();
        }
        else if (agg == 10)
        {
            qDebug() << "Type II.g:";
            random_aggregate_greedy_max_degree();
        }
        else if (agg == 11)
        {
            qDebug() << "Type II.h:";
            random_aggregate_greedy_max_weight();
        }
        else if (agg == 12)
        {
            qDebug() << "Type III.a:";
            random_aggregate_retain_vertex_using_triangulation();
        }
        else if (agg == 13)
        {
            qDebug() << "Type III.b:";
            random_aggregate_retain_vertex_using_probabilistic_triangulation();
        }
        else if (agg == 14)
        {
            qDebug() << "Type III.c:";
            random_aggregate_with_highest_triangulated_vertex();
        }
        else if (agg == 15)
        {
            qDebug() << "Type III.d:";
            random_aggregate_retain_vertex_using_triangulation_times_weight();
        }
        else if (agg == 16)
        {
            qDebug() << "Type III.e:";
            random_aggregate_retain_vertex_using_triangulation_of_cluster();
        }
    }
}

/** Select Type of Aggregation to Run
 * @brief Graph::run_aggregation_on_selection
 * @param n
 */
void Graph::run_aggregation_on_selection(int n)
{
    switch (n) {
    case 0:
        qDebug() << "Type I.a:";
        random_aggregate();
        break;
    case 1:
        qDebug() << "Type I.b:";
        random_aggregate_with_degree_comparison();
        break;
    case 2:
        qDebug() << "Type I.c:";
        random_aggregate_with_weight_comparison();
        break;
    case 3:
         qDebug() << "Type II.a:";
        random_aggregate_with_neighbour_initial_degree_bias();
        break;
    case 4:
        qDebug() << "Type II.b:";
        random_aggregate_with_neighbour_CURRENT_degree_bias();
        break;
    case 5:
        qDebug() << "Type II.c:";
        random_aggregate_highest_CURRENT_degree_neighbour();
        break;
    case 6:
        qDebug() << "Type II.d:";
        random_aggregate_with_minimum_weight_neighbour();
        break;
    case 7:
        qDebug() << "Type II.e:";
        random_aggregate_probabilistic_lowest_degree_neighbour_destructive();
        break;
    case 8:
        qDebug() << "Type II.f:";
        random_aggregate_probabilistic_candidate_with_minimum_weight_neighbour();
        break;
    case 9:
        qDebug() << "Type II.g:";
        random_aggregate_greedy_max_degree();
        break;
    case 10:
        qDebug() << "Type II.h:";
        random_aggregate_greedy_max_weight();
        break;
    case 11:
        qDebug() << "Type III.a:";
        random_aggregate_retain_vertex_using_triangulation();
        break;
    case 12:
        qDebug() << "Type III.b:";
        random_aggregate_retain_vertex_using_probabilistic_triangulation();
        break;
    case 13:
        qDebug() << "Type III.c:";
        random_aggregate_with_highest_triangulated_vertex();
        break;
    case 14:
        qDebug() << "Type III.d:";
        random_aggregate_retain_vertex_using_triangulation_times_weight();
        break;
    case 15:
        qDebug() << "Type III.e:";
        random_aggregate_retain_vertex_using_triangulation_of_cluster();
        break;
    default:
        qDebug() << "Select A Number Between 0 - 15";
      break;
    }
}

void Graph::LARGE_hard_reset()
{
    for(int i = 0; i < myEdgeList.size(); i++)
    {
        delete myEdgeList[i];
    }
    for (int i = 0; i < myVertexList.size(); i++)
    {
        myVertexList[i]->removeAll();
        delete myVertexList[i];
    }
    myVertexList.clear();
    myEdgeList.clear();
    hierarchy.clear();
    centroids.clear();
    large_result.clear();
    graphIsReady = false;
}


/** Calculate Modularity
 * @brief Graph::LARGE_compute_modularity
 * @return
 */
double Graph::LARGE_compute_modularity()
{
    //firstly reload the edges
    if (!graphIsReady)
    {
        clear_edge();
        LARGE_reload_edges();
    }
    global_e = myEdgeList.size();
    //go through result
    double Q = 0.0;
    for (int i = 0; i < large_result.size(); i++)
    {
        QList<quint32> c = large_result[i];
        QSet<quint32> vi = c.toSet();
        quint64 intra = 0, inter = 0;
        for (int j = 0; j < c.size(); j++)
        {
            quint32 id = c[j];
            Vertex * v = myVertexList.at(id);
            QList<Edge*> adj = v->getAllEdge();
            for (int k = 0; k < adj.size(); k++)
            {
                Vertex * other = v->get_neighbour_fromEdge(adj[k]);
                quint32 other_id = other->getIndex();
                if (vi.contains(other_id))
                    intra++;
                else
                    inter++;
            }
        }

        quint64 m = 2*global_e;
        double e = (double)intra/m;
        double a = (double)(intra + inter)/m;
        double Qi = e - qPow(a,2);
        Q += Qi;
    }
    graphIsReady = false;
    return Q;
}

double Graph::LARGE_compute_modularit_for_truth()
{
    if (ground_truth_communities.empty())
    {
        qDebug() << "Community Has Not Been Set";
        return -1;
    }
    //firstly reload the edges
    if (global_e == 0)
    {
        qDebug() << "Graph Has Not Been Initialised Properly: E = 0 ! Trying to Probe Again;";
        global_e = myEdgeList.size();
    }
    //go through result
    double Q = 0.0;
    for (int i = 0; i < ground_truth_communities.size(); i++)
    {
        QList<quint32> c = ground_truth_communities[i];
        QSet<quint32> vi = c.toSet();
        quint64 intra = 0, inter = 0;
        for (int j = 0; j < c.size(); j++)
        {
            quint32 id = c[j];
            Vertex * v = myVertexList.at(id);
            QList<Edge*> adj = v->getAllEdge();
            for (int k = 0; k < adj.size(); k++)
            {
                Vertex * other = v->get_neighbour_fromEdge(adj[k]);
                quint32 other_id = other->getIndex();
                if (vi.contains(other_id))
                    intra++;
                else
                    inter++;
            }
        }

        quint64 m = 2*global_e;
        double e = (double)intra/m;
        double a = (double)(intra + inter)/m;
        double Qi = e - qPow(a,2);
        Q += Qi;
    }
    return Q;

}

quint32 Graph::count_result_connected_component()
{
    typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS> NormalGraph;
    NormalGraph g;
    for(int i = 0; i < myVertexList.size(); i++)
        boost::add_vertex(g);

    for (int i = 0; i < hierarchy.size(); i++)
    {
        QPair<quint32,quint32> p = hierarchy.at(i);
        boost::add_edge(p.first, p.second, g);
    }

    std::vector<quint32> component(boost::num_vertices(g));
    quint32 num = boost::connected_components(g, &component[0]);
    return num;
}



/** Girvan and Newman Fraction of Correctly Classified
 * @brief Graph::compute_GN_index
 * @return
 */

int countCommon(const QList<quint32> &A, const QList<quint32> &B)
{
    QSet<quint32> a = A.toSet(),
                  b = B.toSet();
    return a.intersect(b).size();
}

bool checkMerge(const QList<quint32> &A, const QList<QList<quint32> > &truth)
{
    QSet<quint32> cA = A.toSet();
    int match = 0;
    for(int i = 0 ; i < truth.size(); i++)
    {
        QList<quint32> c = truth.at(i);
        int count = 0;
        for(int j = 0; j < c.size(); j++)
        {
            if (cA.contains(c[j])) count++;
        }
        if (count == c.size()) match++;
    }
    if (match >= 2) return true;
    else return false;
}

double Graph::compute_GN_index()
{
    quint32 n = myVertexList.size();
    //indexing ground truth community
    QMap<quint32,quint32> comm_map; //mapping vertex_id - comm_id
    for (int i = 0; i < large_result.size(); i++)
    {
        for(int j = 0; j < large_result.at(i).size(); j++)
        {
            quint32 vi = large_result.at(i).at(j);
            comm_map.insert(vi,i);
        }

    }
    //indexing result community
    QList<int> v;
    for (int i = 0; i < n; i++)
        v.append(-1);

    for (int i = 0; i < v.size(); i++)
    {
        if (v[i] == -1)
        {
            quint32 truth_comm_index = i/32;
            QList<quint32> truth = ground_truth_communities[truth_comm_index];
            quint32 result_comm_index = comm_map.value(i);
            QList<quint32> result = large_result[result_comm_index];
            if (!result.contains(i) && !truth.contains(i))
            {
                qDebug() << "STRANGE";
                return -1.0;
            }
            //check for merging first
            if (result.size() > 64 && checkMerge(result, ground_truth_communities))
            {
                for(int i = 0; i < result.size(); i++)
                    v[result[i]] = 0;
            }
            else
            {
            //compare matching
                int common = countCommon(truth, result);
                int correct = common - 1;
                if (correct < truth.size() / 2)
                {
                    v[i] = 0;
                }
                else if (correct >= truth.size() / 2)
                {
                    v[i] = 1;
                }
            }
        }
    }

    int match = 0;
    for (int i = 0; i < v.size(); i++)
    {
        if (v[i] < 0)
            qDebug() << "Newman Index - ERROR Potential Duplication/Un-parsed result";
        else
            match += v[i];
    }
    double newman_index = (double)match/n;
    printf("Girvan Newman Fraction of Correctly Classified: %f", newman_index);
    return newman_index;
}

// --------------------------- POST AGGREGATION -----------------------------------------
// --------------------------------------------------------------------------------------
/** For each cluster in the results from previous aggregation,
 * Each cluster is now collapsed into a super vertex, which is then used for further aggregation
 * @brief Graph::PostAgg_generate_super_vertex
 */
void Graph::PostAgg_generate_super_vertex()
{
    if(large_result.empty())
    {
        qDebug() << "- Aggregation Result is Empty! Terminating ...";
        return;
    }
    if (myEdgeList.empty()) //reload edge if this is not retain type
    {
        LARGE_reload_edges();
    }
    //first create super vertices
    QList<Vertex*> superV;
    QMap<int,Vertex*> superMap; //map from old vertex to new vertex

    for(int i = 0; i < large_result.size(); i++)
    {
        Vertex * v = new Vertex;
        v->setIndex(superV.size());
        superV.append(v);
        quint32 size = 0;
        QList<quint32> c = large_result[i];
        if (no_run == 0)
            size += c.size();
        for (int j = 0; j < c.size(); j++)
        {
            quint32 id = c[j];
            superMap.insert(id,v);
            Vertex * child = myVertexList[id];
            size += child->getcSize();
        }
        v->setcSize(size);
    }
    QList<Edge*> superE;
    QList<QPair<quint32,quint32> > newE;
    //conencting super vertices
    for(int i = 0; i < myEdgeList.size(); i++)
    {
        quint32 from = myEdgeList[i]->fromVertex()->getIndex(), to = myEdgeList[i]->toVertex()->getIndex();
        if (!superMap.contains(from) || !superMap.contains(to))
        {
            qDebug() << "- Post Aggregation Error! Vertices Has Not Been Assigned To A Super Vertex";
            qDebug() << "- Terminating ...";
            return;
        }
        Vertex * superFrom = superMap.value(from);
        Vertex * superTo = superMap.value(to);
        if (superFrom->getIndex() == superTo->getIndex()){
        }
        else
        {
            QPair<quint32,quint32> s = qMakePair(superFrom->getIndex(), superTo->getIndex());
            QPair<quint32,quint32> r_s = qMakePair(superTo->getIndex(), superFrom->getIndex());
            if (!newE.contains(s) && !newE.contains(r_s))
            {
                newE.append(s);
                newE.append(r_s);
                Edge * e = new Edge(superFrom,superTo,superE.size());
                superE.append(e);
            }
        }
    }
    save_current_clusters();
    //clearing the old list
    for (int i = 0; i < myVertexList.size(); i++)
        delete myVertexList[i];
    myVertexList = superV;
    myEdgeList = superE;
    QMap<quint32,QString> colourMap;
    mapping_colour_to_cluster(colourMap);
    print_result_community__with_attributes_inGraphML(colourMap);
    qDebug() << "- Post Aggregation Finished! After collapsing: SuperV: " << myVertexList.size()
             << "SuperE: " << myEdgeList.size();
    global_e = myEdgeList.size();
    global_v = myVertexList.size();
    no_run++;
    qDebug() << "After Clustering Coefficient:" << cal_average_clustering_coefficient();
    qDebug() << "Saving to the dir: " << globalDirPath;
    save_current_run_as_edge_file(QString(globalDirPath + "/superGraph" + QString::number(no_run) + ".txt"));
    save_current_run_summary_file(QString(globalDirPath + "/summary_superGraph" + QString::number(no_run) + ".txt"));
}

/**
 * @brief Graph::sort_cluster_by_size
 * @param list
 */
void Graph::mapping_colour_to_cluster(QMap<quint32,QString> &colour)
{
    qDebug() << "HERE";
    QList<QString> col;
    QString a = QString("F6511D");
    QString b = QString("FF8805");
    QString c = QString("FF5584");
    QString d = QString("00bd94");
    QString e = QString("d3b3b0");
    QString f = QString("dF89F");
    QString g = QString("73c000");
    QString h = QString("4c463e");
    col << a << b << c << d << e << f << g << h;
    QList<quint32> size_list, copy;
    for (int i = 0; i < myVertexList.size(); i++)
        size_list.append(myVertexList[i]->getcSize());
    copy = size_list;
    qSort(copy.begin(),copy.end());
    //get few highest
    int num = col.size();
    if (copy.size() < col.size())
        num = copy.size();
    for (int i = 0; i < num; i++)
    {
        quint32 largest = copy.at(copy.size()-1-i);
        quint32 index = size_list.indexOf(largest);
        colour.insert(index,col[i]);
    }
}

/** Save the current run to stich back later
 * @brief Graph::save_current_clusters
 */
void Graph::save_current_clusters()
{
    //sub clusters are always saved into the SubCluster Folder
    QString subCDir(globalDirPath + "/SubCluster/");
    QDir dir(subCDir);
    if (dir.exists()){}
    else
        QDir().mkdir(subCDir);
    QString fileSubfix("SubCluster_level" + QString::number(no_run) + ".txt");
    QFile outFile(subCDir+fileSubfix);
    outFile.open(QIODevice::WriteOnly|QIODevice::Text);
    QTextStream out(&outFile);
    for (int i = 0; i < large_result.size(); i++)
    {
        QList<quint32> c = large_result[i];
        for (int j = 0; j < c.size(); j++)
        {
            out << c[j];
            if (j < c.size()-1)
                out << '\t';
        }
        if (i == large_result.size() - 1)
            break;
        out << '\n';
    }
    outFile.close();
}


/** MERGE RESULT CLUSTERS FROM FILES <FASTER>
 * @brief Graph::merge_result_clusters
 */
void Graph::merge_result_clusters(const int &limit)
{
    QString subCDir(globalDirPath + "SubCluster/");
    QDir dir(subCDir);
    if (!dir.exists() || dir.entryInfoList().empty()){
        qDebug() << "- Sub Cluster Dir is Not Properly Configured";
        return;
    }
    QStringList filters;
    filters << "*.txt";
    QFileInfoList fileinfo = dir.entryInfoList(filters);
    QList<QString> toread;
    QString str = "";
    for (int i = 0; i < limit; i++)
        toread << str;
    for (int i = 0; i < fileinfo.size(); i++)
    {
        QString name = fileinfo[i].fileName();
        QString path = fileinfo[i].absoluteFilePath();
        if (!name.contains(QString("SubCluster")))
            qDebug() << "While Loading SubCluster: Unidentified File Found";
        else
        {
            int l = get_number_from_filename(name);
            if (l >= 0)
            {
                if (l < limit)
                    toread.replace(l,path);
            }
        }
    }
    //parse the tree, starting from the bottom and stop at the desired level
    parse_tree(toread);
}

void Graph::manual_set_working_dir(QString dirPath)
{
    globalDirPath = dirPath;
}


/** Get to the desired level of the tree
 * @brief Graph::parse_tree
 * @param file
 */
void Graph::parse_tree(const QList<QString> &file)
{
    //starting from the bottom
    //firstly reconstruct the graph
    read_simple_edge(globalDirPath);
    //bottom level of hierarchy is done seperately, then afterward just merge the bottom levels
    QList<QList<quint32> > lower;
    //load the bottom level
    QString filename = file.at(0);
    get_one_level_cluster(filename, lower);
    large_result = lower;
    // OK Getting to top/desired level
    int current_level = 1, total_levels = file.size();
    while (current_level < total_levels)
    {
        QList<QList<quint32> > currentCluster;
        QString curLevelFile = file.at(current_level); //file lab
        get_one_level_cluster(curLevelFile, currentCluster); //get the next level cluster
        //now merge large_result with next level cluster
        QList<QList<quint32> > newResult;
        for (int i = 0 ; i < currentCluster.size(); i++)
        {
            QList<quint32> parentC = currentCluster[i];
            QList<quint32> mergeC;
            for (int j = 0; j < parentC.size(); j++)
            {
                int childId = parentC[j];
                QList<quint32> originC = large_result[childId];
                mergeC.append(originC);
            }
            newResult.append(mergeC);
        }
        large_result = newResult;
        current_level++;
    }
    //Check Sum
    qDebug() << "Tree Parsed Successfully!";
}

/** Read a community file and assign it to clus
 * @brief Graph::get_one_level_cluster
 * @param filename
 * @param clus
 */
void Graph::get_one_level_cluster(QString filename, QList<QList<quint32> > &clus)
{
    QFile file(filename);
    if (!file.exists())
    {
        qDebug() << "- While Parsing Hierarchy Tree: File Not Found! Terminating ...";
        return;
    }
    file.open(QIODevice::ReadOnly|QIODevice::Text);
    QTextStream in(&file);
    QSet<quint32> unique_id;
    QStringList all_c = in.readAll().split('\n');
    for (int i = 0; i < all_c.size(); i++)
    {
        QStringList a_c = all_c[i].split('\t');
        QList<quint32> comm;
        for (int j = 0; j < a_c.size(); j++)
        {
            bool ok;
            quint32 id = a_c[j].toUInt(&ok);
            if (ok)
            {
                comm.append(id);
                if(unique_id.contains(id))
                    qDebug() << id;
                else
                    unique_id.insert(id);
            }
            else
                qDebug() << "- While Parsing Hierarchy Tree: Vertex Index Error";
        }
        clus.append(comm);
    }
}

/**  Select and save a community
 * @brief Graph::ReAgg_select_and_save_a_community
 * @param k
 */
void Graph::ReAgg_select_and_save_community(int k)
{
    if (k > large_result.size() || k < 0)
    {
        qDebug() << "- While fetching a community: number invalid!";
        return;
    }
    QList<quint32> c = large_result.at(k);
    print_single_community_inGraphML(c, k);
}

/** Select and save multiple communities
 * @brief Graph::ReAgg_select_and_save_a_community
 * @param list
 */
void Graph::ReAgg_select_and_save_community(const QList<quint32> &list)
{
    for (int i = 0; i < list.size(); i++)
    {
        if (list[i] < 0 || list[i] > large_result.size())
        {
            qDebug() << "- While Saving Multiple Communities: Community ID Out of Range! Terminating ...";
            return;
        }
    }
    print_multiple_communities_inGraphML(list);
}

void Graph::ReAgg_select_and_save_community_with_intra_edge(const quint32 &size_minimum)
{
    QList<quint32> id;
    for (int i = 0; i < large_result.size(); i++)
    {
        quint32 s = large_result[i].size();
        if (s > size_minimum )
        {
            id.append(i);
        }
    }
    for (int i = 0; i < id.size(); i++)
        print_single_community_with_only_intra_edge_inGraphML(id[i]);
}


void Graph::ReAgg_select_and_save_community_by_size(const quint32 &size_minimum)
{
    QList<quint32> list;
    for (int i = 0; i < large_result.size(); i++)
    {
        quint32 s = large_result[i].size();
        if (s > size_minimum )
            list.append(i);
    }
    print_multiple_communities_inGraphML(list);
}



/** Colour Converter
 * @brief Graph::RGB_converter
 * @param hexValue
 * @return
 */
QList<quint32> Graph::RGB_converter(quint32 c)
{
    QList<quint32> col;
    quint32 r,g,b;
    r =  (c & 0xff0000) >> 16;  // Extract the RR byte
    g = (c & 0x00ff00) >> 8;   // Extract the GG byte
    b = (c & 0x0000ff);        // Extract the BB byte
    col << r << g << b;
    return col;
}

/** Print to console comm stats
 * @brief Graph::ReAgg_print_communities_stats
 */
void Graph::ReAgg_print_communities_stats()
{
    qDebug() << "Result Communities: ";
    for (int i = 0; i < large_result.size(); i++)
    {
        if (large_result[i].size() > 3)
            qDebug() << "#" << i << ": " << large_result[i].size();
    }
}


// --------------------------- COLOURING ------------------------------
/** Calling Method
 * @brief Graph::colouring_cluster_result
 */
void Graph::colouring_cluster_result()
{
    qDebug() << "- Preparing for Projecting Clusters Hierarchies ...";
    //check dir
    QDir projectDir(globalDirPath + "/Projecting/");
    if (!projectDir.exists())
        QDir().mkdir(globalDirPath + "/Projecting/");
    //check supercluster
    QDir subclusterDir(globalDirPath + "/SubCluster/");
    if (!subclusterDir.exists() || subclusterDir.count() == 0)
    {
        qDebug() << "- Sub Cluster Dir is Empty";
        return;
    }
    QFileInfoList fileList = subclusterDir.entryInfoList();
    int noFile = fileList.count();
    //checking level
    int highest = 0, count = -1;
    QMap<int,QString> folder;
    for (int i = 0 ;i  < noFile; i++)
    {
        QFileInfo file = fileList[i];
        QString name = file.fileName();
        if (name.contains("SubCluster"))
        {
            QString level = name.at(name.length());
            bool ok;
            int l = level.toUInt(&ok);
            if (ok)
            {
                folder.insert(l, file.absoluteFilePath());
                if (l > highest)
                    highest = l;
                count++;
            }
        }
    }
    if (count != highest)
    {
        qDebug() << "- While Parsing for Projecting Clusters: Missing A SubCluster!";
        return;
    }
    QList<QPair<QString,QString> > toParse;

    qDebug() << "- Files Are OK! Starting ...";
    project_higher_levels_on_lower_levels(toParse);
}

/** Locate File in Dir Path
 * @brief Graph::locate_file_in_dir
 * @param filePath
 * @return false: if not found
 * other wise modify the filepath
 */
bool Graph::locate_file_in_dir(QString &fileName)
{
    QDir dir(globalDirPath);
    if (!dir.exists() || globalDirPath.size() == 0)
        return false;
    QFileInfoList file_list = dir.entryInfoList();
    for (int i = 0; i < file_list.size(); i++)
    {
        QFileInfo file = file_list.at(i);
        QString name = file.fileName();
        if (name.contains(fileName))
        {
            fileName = file.absoluteFilePath();
            return true;
        }
    }
    return false;
}

int Graph::get_number_from_filename(QString filename)
{
    QStringList str = filename.split(".");
    QString name = str[0];
    QList<QChar> charlist;
    for (int i = 0; i< name.size(); i++)
        charlist.append(name[i]);
    QString level;
    QListIterator<QChar> i(charlist);
    i.toBack();
    while (i.hasPrevious())
    {
        QChar c = i.previous();
        if (c.isDigit())
        {
            level.prepend(c);
        }
        else
            break;
    }
    bool ok;
    int l = level.toInt(&ok);
    if (ok)
        return l;
    else
    {
        qDebug() << "- Error While Extracting Number from FileName!";
        return -1;
    }
}



/** THe Higher Level is Coloured and then this coloured is projected onto lower level
 * @brief Graph::project_higher_levels_on_lower_levels
 * @param higher
 * @param lower
 */
void Graph::project_higher_levels_on_lower_levels(const QList<QPair<QString,QString> > &toParse)
{

}


/** Reconstruct the graph at the desired level
 * @brief Graph::construct_hierarchy_graph
 * @param level: the desired level: the level wanted, files to be read are:
 * 1. superGraph for edge and vertex and
 * 2. subcluster (at level+1) for cluster result
 */
void Graph::construct_graph_at_a_specific_level(int level)
{
    //check if the graph level exists
    qDebug() << QString("- Rebuilding Graph at Hierarchy Level %1 ...").arg(level);
    QDir dir(globalDirPath);
    if (!dir.exists() || globalDirPath.size() == 0)
    {
        qDebug() << "- While Rebuilding Hierarchy Graphs: Dir not Exists or Global Dir Path has not been set";
        return;
    }
    //looking for the file in SuperGraphx.txt
    QFileInfoList list = dir.entryInfoList();
    QString summary = "", edge = "";
    for (int i = 0; i < list.size(); i++)
    {
        QFileInfo file = list[i];
        QString name = file.fileName();
        if (name.contains("summary_superGraph"))
        {
            QString strlevel = name.split(".").at(0);
            bool ok;
            strlevel = strlevel.at(strlevel.length()-1);
            int this_level = strlevel.toInt(&ok);
            if (ok && this_level == level)
                summary = file.absoluteFilePath();
        }
        else if (name.contains("superGraph"))
        {
            QString strlevel = name.split(".").at(0);
            bool ok;
            strlevel = strlevel.at(strlevel.length()-1);
            int this_level = strlevel.toInt(&ok);
            if (ok && this_level == level)
                edge = file.absoluteFilePath();
        }
    }
    if (summary.size() == 0 || edge.size() == 0)
    {
        qDebug() << "- While Rebuilding Herarchy Graphs: The Desired Graph Level Cannot be Found";
        return;
    }
    //reload edges
    read_superGraph(edge, summary);
    //graph loaded, load the result cluster
    QDir dir2(globalDirPath + "/SubCluster/");
    if (!dir2.exists() || globalDirPath.size() == 0)
    {
        qDebug() << "- While Rebuilding Hierarchy Graphs\n- While Loading SubCluster : Dir not Exists or Global Dir Path has not been set";
        return;
    }
    //looking for the file in SuperGraphx.txt
    QFileInfoList list2 = dir2.entryInfoList();
    QString toread = "";
    for (int i = 0; i < list2.size(); i++)
    {
        QFileInfo file = list2[i];
        QString name = file.fileName();
        if (name.contains("SubCluster"))
        {
            QString strlevel = name.split(".").at(0);
            bool ok;
            strlevel = strlevel.at(strlevel.length()-1);
            int this_level = strlevel.toInt(&ok);
            if (ok && this_level == (level))
                toread = file.absoluteFilePath();
        }
    }
    if (toread.size() == 0)
    {
        qDebug() << "- While Rebuilding Hierarchy Graphs\n- While Loading SubCluster: The Desired Graph Level Cannot be Found";
        return;
    }
    read_SuperCluster(toread);
}

/** Reload The SuperCluster
 * @brief Graph::read_SuperCluster
 * @param filePath
 */
void Graph::read_SuperCluster(QString filePath)
{
    QFile file(filePath);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QTextStream in(&file);
    QStringList c_list = in.readAll().split('\n');
    quint32 checksum = 0;
    for (int i = 0; i < c_list.size(); i++)
    {
        QStringList c = c_list[i].split('\t');
        checksum+=c.size();
        QList<quint32> comm;
        for (int j = 0; j < c.size(); j++)
        {
            bool ok;
            quint32 id = c[j].toUInt(&ok);
            qDebug() << id;
            if (!ok)
            {
                qDebug() << "- While Rebuilding Hierarchy Graphs\n- While Loading SubCluster: Index Error ";
                return;
            }
            Vertex * v = myVertexList.at(id);
            v->setTruthCommunity(i);
            comm.append(id);
        }
        large_result.append(comm);
    }
    if (checksum != myVertexList.size())
    {
        qDebug() << "- While Rebuilding Hierarchy Graphs\n- While Loading SubCluster: CheckSum Fails!";
        return;
    }
    else
    {
        qDebug() << QString("- Reconstruction Complete for Hierachy at Level %1").arg(no_run);
        graphIsReady = true;
        print_result_stats();
    }
}
