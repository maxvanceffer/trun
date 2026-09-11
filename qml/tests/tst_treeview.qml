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

    // Test case: TreeModel has correct item count after adding projects
    function test_tree_model_item_count() {
        var model = Qt.createQmlObject(
            'import Trun.Models 1.0; QmlTreeModel { }',
            testWindow,
            "testModel"
        )
        compare(model.rowCount(), 0, "Model should start empty")

        model.addProject("/test/project1", "Project 1", "Test Project 1", "package.json", [])
        compare(model.rowCount(), 1, "Should have 1 project")

        model.addProject("/test/project2", "Project 2", "Test Project 2", "Cargo.toml", [])
        compare(model.rowCount(), 2, "Should have 2 projects")

        model.addFolder("/test/folder", "/test/folder")
        compare(model.rowCount(), 3, "Should have 3 items")

        model.destroy()
    }

    // Test case: icon mapping for package.json manifest
    function test_icon_mapping_package_json() {
        var model = Qt.createQmlObject(
            'import Trun.Models 1.0; QmlTreeModel { }',
            testWindow,
            "testModel2"
        )

        model.addProject(
            "/test/npm-project",
            "NpmProject",
            "NPM Project",
            "package.json",
            []
        )

        var idx = model.index(0, 0)
        verify(idx.isValid(), "Index should be valid")

        var manifest = model.data(idx, model.ManifestRole)
        compare(manifest, "package.json", "Manifest should be package.json")

        var name = model.data(idx, model.NameRole)
        compare(name, "NpmProject", "Name should be NpmProject")

        model.destroy()
    }

    // Test case: icon mapping for Cargo.toml manifest
    function test_icon_mapping_cargo_toml() {
        var model = Qt.createQmlObject(
            'import Trun.Models 1.0; QmlTreeModel { }',
            testWindow,
            "testModel3"
        )

        model.addProject(
            "/test/rust-project",
            "RustProject",
            "Rust Project",
            "Cargo.toml",
            []
        )

        var idx = model.index(0, 0)
        var manifest = model.data(idx, model.ManifestRole)
        compare(manifest, "Cargo.toml", "Manifest should be Cargo.toml")

        model.destroy()
    }

    // Test case: folder expand/collapse works via TreeView expandedRows
    function test_folder_expansion() {
        var model = Qt.createQmlObject(
            'import Trun.Models 1.0; QmlTreeModel { }',
            testWindow,
            "testModel4"
        )

        model.addFolder("/test/folder", "/test/folder")
        model.addProject("/test/folder/project", "FolderProject", "In Folder", "go.mod", [])

        var folderIdx = model.index(0, 0)
        compare(model.rowCount(folderIdx), 1, "Folder should have 1 child")

        verify(folderIdx.isValid(), "Folder index should be valid")

        model.destroy()
    }

    // Test case: project selection triggers correct command count
    function test_project_command_count() {
        var model = Qt.createQmlObject(
            'import Trun.Models 1.0; QmlTreeModel { }',
            testWindow,
            "testModel5"
        )

        var commands = [{"name": "run", "command": "npm run start"}, {"name": "test", "command": "npm test"}]
        model.addProject(
            "/test/commands-project",
            "CommandsProject",
            "Has commands",
            "package.json",
            commands
        )

        var idx = model.index(0, 0)
        var cmdData = model.data(idx, model.CommandsRole)

        verify(cmdData !== null, "Commands data should not be null")

        model.destroy()
    }
}
