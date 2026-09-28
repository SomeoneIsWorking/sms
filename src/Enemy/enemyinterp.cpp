#include <Enemy/EnemyInterp.hpp>
#include <Strategic/LiveActor.hpp>

static void einGetLengthToMario(TSpcTypedInterp<TLiveActor>* interp,
                                u32 arg_num)
{
	interp->push();
}

static void einGetLengthToCurPathNode(TSpcTypedInterp<TLiveActor>* interp,
                                      u32 arg_num)
{
	interp->push();
}

static void einGoToRandomNextGraphNode(TSpcTypedInterp<TLiveActor>* interp,
                                       u32 arg_num)
{
	interp->push();
}

static void einWalkToCurPathNode(TSpcTypedInterp<TLiveActor>* interp,
                                 u32 arg_num)
{
	interp->push();
}

static void einSetGraph(TSpcTypedInterp<TLiveActor>* interp, u32 arg_num)
{
	interp->push();
}

void TEinBinary::initUserBuiltin()
{
	TSpcTypedBinary<TLiveActor>::initUserBuiltin();
	bindSystemDataToSymbol("setGraph", (void*)&einSetGraph);
	bindSystemDataToSymbol("walkToCurPathNode", (void*)&einWalkToCurPathNode);
	bindSystemDataToSymbol("goToRandomNextGraphNode",
	                       (void*)&einGoToRandomNextGraphNode);
	bindSystemDataToSymbol("getLengthToCurPathNode",
	                       (void*)&einGetLengthToCurPathNode);
	bindSystemDataToSymbol("getLengthToMario", (void*)&einGetLengthToMario);
}
