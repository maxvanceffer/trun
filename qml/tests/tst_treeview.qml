import QtQuick 6.5
import Trun.Models 1.0

TestCase {
    name: "TreeView"

    // Shared test window for all test cases
    QtObject {
        id: testWindow
        width: 800
        height: 600
    }

    // Test case: TreeModel has correct item count after adding manifests.
    // The tree shows folders only; manifests live on folder nodes.
    function test_tree_model_item_count() {
        var model = Qt.createQmlObject(
            'import Trun.Models 1.0; QmlTreeModel { }',
            testWindow,
            "testModel"
        )
        compare(model.rowCount(), 0, "Model should start empty")

        model.setRootPath("/test")
        model.addProjectManifest("/test/project1", "package.json")
        compare(model.rowCount(), 1, "Should have 1 root")

        model.addProjectManifest("/test/project2", "Cargo.toml")
        var rootIdx = model.index(0, 0)
        compare(model.rowCount(rootIdx), 2, "Root should have 2 folders")

        model.destroy()
    }

    // Test case: leaf folders with manifests navigate directly (no children)
    function test_leaf_folder_has_manifests_flag() {
        var model = Qt.createQmlObject(
            'import Trun.Models 1.0; QmlTreeModel { }',
            testWindow,
            "testModel2"
        )

        model.setRootPath("/test")
        model.addProjectManifest("/test/npm-project", "package.json")

        var rootIdx = model.index(0, 0)
        verify(rootIdx.isValid(), "Root index should be valid")

        var folderIdx = model.index(0, 0, rootIdx)
        verify(folderIdx.isValid(), "Folder index should be valid")
        compare(model.data(folderIdx, model.ItemTypeRole), "folder")
        compare(model.data(folderIdx, model.HasManifestsRole), true)
        compare(model.rowCount(folderIdx), 0, "Leaf folder has no children")

        model.destroy()
    }

    // Test case: hybrid folders (manifests plus subfolders) grow one entry
    function test_hybrid_folder_manifests_entry() {
        var model = Qt.createQmlObject(
            'import Trun.Models 1.0; QmlTreeModel { }',
            testWindow,
            "testModel3"
        )

        model.setRootPath("/test")
        model.addProjectManifest("/test/mixed", "package.json")
        model.addProjectManifest("/test/mixed/sub", "go.mod")

        var mixedIdx = model.index(0, 0, model.index(0, 0))
        verify(mixedIdx.isValid(), "Mixed folder index should be valid")
        compare(model.rowCount(mixedIdx), 2, "Entry plus one subfolder")

        var entryIdx = model.index(0, 0, mixedIdx)
        compare(model.data(entryIdx, model.ItemTypeRole), "manifests")
        compare(model.data(entryIdx, model.FolderPathRole), "/test/mixed")
        compare(model.data(entryIdx, model.ManifestRole), "package.json")

        model.destroy()
    }

    // Test case: entry icon becomes the stack on the second manifest
    function test_manifests_entry_stack() {
        var model = Qt.createQmlObject(
            'import Trun.Models 1.0; QmlTreeModel { }',
            testWindow,
            "testModel4"
        )

        model.setRootPath("/test")
        model.addProjectManifest("/test/mixed", "package.json")
        model.addProjectManifest("/test/mixed/sub", "go.mod")
        model.addProjectManifest("/test/mixed", "composer.json")

        var entryIdx = model.index(0, 0, model.index(0, 0, model.index(0, 0)))
        compare(model.data(entryIdx, model.ItemTypeRole), "manifests")
        compare(model.data(entryIdx, model.ManifestRole), "",
            "Several manifests: stack icon, no single name")

        model.destroy()
    }

    // Test case: folders nest implicitly for nested manifest paths
    function test_folder_expansion() {
        var model = Qt.createQmlObject(
            'import Trun.Models 1.0; QmlTreeModel { }',
            testWindow,
            "testModel5"
        )

        model.setRootPath("/test")
        model.addProjectManifest("/test/folder/project", "go.mod")

        var folderIdx = model.index(0, 0, model.index(0, 0))
        verify(folderIdx.isValid(), "Folder index should be valid")
        compare(model.data(folderIdx, model.ItemTypeRole), "folder")
        compare(model.data(folderIdx, model.HasManifestsRole), false)
        compare(model.rowCount(folderIdx), 1, "Folder should have 1 child")

        model.destroy()
    }
}
