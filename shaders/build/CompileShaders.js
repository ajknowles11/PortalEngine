if (process.argv.length < 5)
{
    // error
    console.log("Error: not enough args supplied!\n" +
                "Usage is: 'node <script> <glslc exe> <input dir> <output dir> [optional]'\n" +
                "Optional flags:\n" +
                "'--noTimestamps': compile and copy all shaders without checking or saving timestamps (for shipping builds)");
}
else
{
    const glslc = process.argv[2];
    const inputDir = process.argv[3] + '/';
    const outputDir = process.argv[4] + '/';

    const useTimestamps = process.argv.length < 6 || process.argv[5] != '--noTimestamps';

    const timestampFile = outputDir + 'ShaderCompileTimes.json';
    var timestampMap = new Map();

    const fs = require('fs');

    // OPEN TIMESTAMP FILE

    if (useTimestamps)
    {
        try
        {
            var data = fs.readFileSync(timestampFile)
            if (!data)
            {
                return;
            }

            var result = JSON.parse(data);
            if (result.length == 0)
            {
                return;
            }

            timestampMap = new Map(Object.entries(result));
        }
        catch (err)
        {
            if (err.code == 'ENOENT') // file doesn't exist
            {
                // We'll make it later
                console.log("Shader compilation timestamp file not found, creating one.")
            }
            else
            {
                throw err;
            }
        }
    }

    // COMPILE MODIFIED SHADERS

    const exec = require('child_process').execFile;
    const path = require('path');

    const validExts = ['.frag', '.vert', '.comp'];

    // Resolve undefined if we skip file (either not a shader, or already up-to-date)
    // Resolve with timestamp map entry (file, mtime) if shader compiled
    // Reject if error (compilation failed)
    function tryCompileShader(dir, file)
    {
        return new Promise((resolve, reject) =>
        {
            if (file == 'build' || validExts.indexOf(path.extname(file)) < 0)
            {
                // Skip irrelevant dir or file
                resolve();
                return;
            }
            else
            {
                relFile = dir + file;

                // Compare edit time with dictionary
                var stats = fs.statSync(relFile);
                var mtime = stats.mtime.getTime();

                if (!useTimestamps || !timestampMap.has(file) || mtime != timestampMap.get(file))
                {
                    // Compile the shader to spv in output directory
                    var spv = outputDir + file + '.spv';
                    exec(glslc, [relFile, '-o', spv], function (err, data)
                    {
                        if (err)
                        {
                            console.error("Error compiling shader %s, reason: %s", file, err.message);
                            reject(err);
                            return;
                        }

                        console.log("Compiled shader %s", file);

                        resolve([file, mtime]);
                        return;
                    });
                }
                else
                {
                    resolve();
                    return;
                }
            }
        });
    }

    const files = fs.readdirSync(inputDir);
    Promise.allSettled(files.map(file => tryCompileShader(inputDir, file))).then((results) =>
    {
        // SAVE NEW TIMESTAMPS TO FILE
        var anyModified = false;
        results.forEach(result =>
        {
            if (result.status == "fulfilled" && result.value != undefined)
            {
                anyModified = true;
                if (useTimestamps)
                {
                    timestampMap[result.value[0]] = result.value[1];
                }
            }
        });

        if (anyModified)
        {
            if (useTimestamps)
            {
                fs.writeFile(timestampFile, JSON.stringify(timestampMap), function (err)
                {
                    if (err)
                    {
                        throw err;
                    }
                    console.log("Compilation completed. Timestamp file updated.");
                });
            }
            else
            {
                console.log("Compilation completed.");
            }
        }
        else
        {
            console.log("No shader changes detected.");
        }
    });
}