// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using Internal.Text;
using Internal.ReadyToRunConstants;
using System.Collections.Generic;
using System;

namespace ILCompiler.DependencyAnalysis.ReadyToRun
{
    /// <summary>
    /// This node emits a thunk calling DelayLoad_Helper with a given instance signature
    /// to populate its indirection cell.
    /// </summary>
    public partial class ImportThunk : AssemblyStubNode, ISymbolDefinitionNode
    {
        enum Kind
        {
            Eager,
            Lazy,
            DelayLoadHelper,
            VirtualStubDispatch,
        }

        private readonly Import _helperCell;

        private readonly Import _instanceCell;

        private readonly ISymbolNode _moduleImport;

        private readonly Kind _thunkKind;

        public ImportThunk(ReadyToRunHelper helperId, NodeFactory factory, Import instanceCell, bool useVirtualCall)
        {
            _helperCell = factory.GetReadyToRunHelperCell(helperId);
            _instanceCell = instanceCell;
            _moduleImport = factory.ModuleImport;

            if (useVirtualCall)
            {
                _thunkKind = Kind.VirtualStubDispatch;
            }
            else if (helperId == ReadyToRunHelper.GetString)
            {
                _thunkKind = Kind.Lazy;
            }
            else if (helperId == ReadyToRunHelper.DelayLoad_MethodCall ||
                helperId == ReadyToRunHelper.DelayLoad_Helper ||
                helperId == ReadyToRunHelper.DelayLoad_Helper_Obj ||
                helperId == ReadyToRunHelper.DelayLoad_Helper_ObjObj)
            {
                _thunkKind = Kind.DelayLoadHelper;
            }
            else
            {
                _thunkKind = Kind.Eager;
            }

            factory.DelayLoadMethodCallThunks.AddEmbeddedObject(this);
        }

        public override void AppendMangledName(NameMangler nameMangler, Utf8StringBuilder sb)
        {
            sb.Append("DelayLoadHelper->");
            _instanceCell.AppendMangledName(nameMangler, sb);
        }

        protected override string GetName(NodeFactory factory)
        {
            Utf8StringBuilder sb = new Utf8StringBuilder();
            AppendMangledName(factory.NameMangler, sb);
            return sb.ToString();
        }

        public override int ClassCode => 433266948;

        public override int CompareToImpl(ISortableNode other, CompilerComparer comparer)
        {
            ImportThunk otherNode = (ImportThunk)other;
            int result = ((int)_thunkKind).CompareTo((int)otherNode._thunkKind);
            if (result != 0)
                return result;

            result = comparer.Compare(_helperCell, otherNode._helperCell);
            if (result != 0)
                return result;

            return comparer.Compare(_instanceCell, otherNode._instanceCell);
        }

        public override IEnumerable<DependencyListEntry> GetStaticDependencies(NodeFactory context)
        {
            if (_helperCell is ExternalMethodImport emi && emi.Method != null && emi.Method.ToString().Contains("Kernel32.GetMessage"))
            {
                Console.WriteLine($"ImportThunk.GetStaticDependencies for {emi.Method.ToString()}");
            }

            if (_instanceCell is ExternalMethodImport emi2 && emi2.Method != null && emi2.Method.ToString().Contains("Kernel32.GetMessage"))
            {
                Console.WriteLine($"ImportThunk.GetStaticDependencies for {emi2.Method.ToString()}");
            }

            return new DependencyListEntry[]
            {
                new DependencyListEntry(_helperCell, "R2R delay load helper import cell"),
                new DependencyListEntry(_instanceCell, "R2R target import cell")
            };
        }
    }
}
