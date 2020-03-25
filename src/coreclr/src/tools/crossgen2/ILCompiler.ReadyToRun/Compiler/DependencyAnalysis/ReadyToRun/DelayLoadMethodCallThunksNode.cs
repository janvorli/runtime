using System;
using System.Collections.Generic;
using System.Text;
using ILCompiler.DependencyAnalysis;
using ILCompiler.DependencyAnalysis.ReadyToRun;

namespace ILCompiler.ReadyToRun.Compiler.DependencyAnalysis.ReadyToRun
{
    public class DelayLoadMethodCallThunksNode : ArrayOfEmbeddedDataNode<ImportThunk>
    {
        public DelayLoadMethodCallThunksNode() : base("DelayLoadMethodCallThunksStart", "DelayLoadMethodCallThunksEnd", nodeSorter: new EmbeddedObjectNodeComparer(new CompilerComparer()))
        {

        }

        public override int ClassCode => (int)ObjectNodeOrder.DelayLoadMethodCallThunksNode;

        protected internal override int Phase => (int)ObjectNodePhase.Ordered;
    }
}
