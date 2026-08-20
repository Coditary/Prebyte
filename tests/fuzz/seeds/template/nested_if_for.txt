{{ if groups }}{{ for group in groups }}{{ if group.featured }}x{{ elseif group.archived }}y{{ else }}z{{ endif }}{{ endfor }}{{ endif }}
