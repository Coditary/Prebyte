local layout = dofile(context.paths.controlDir .. "/scripts/layout.lua")
local paths = layout.paths(context)

if context.fs.exists(paths.symlink_path) then
  local current_target = layout.read_symlink(context, paths.symlink_path)
  if current_target == paths.binary_path then
    local ok, error_message = layout.remove_path(context, paths.symlink_path)
    if not ok then
      context.tx.failed(error_message)
      return false
    end
  end
end

return true
